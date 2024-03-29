// Copyright (c) 2012 Oliver Lau <oliver@von-und-fuer-lau.de>
// All rights reserved.

#include <QtGlobal>
#include <QDesktopServices>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QDialog>
#include <QPalette>
#include <QTextStream>
#include <QSettings>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>
#include <QThread>
#include <QRegExp>
#include <QStringList>
#include <limits>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "random/rnd.h"
#include "dna.h"
#include "breedersettings.h"
#include "main.h"
#include "helper.h"


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mAutoStopTimerId(0)
    , mCloseOnStop(false)
    , mNoDialogs(false)
    , mRecentEvolvedGeneration(0)
    , mRecentEvolvedSelection(0)
{
    QSettings::setDefaultFormat(QSettings::NativeFormat);

    ui->setupUi(this);
    setWindowTitle(tr("%1 %2").arg(AppName).arg(AppVersion));

    RAND::initialize();

    mImageWidget = new ImageWidget;
    QHBoxLayout* hbox1 = new QHBoxLayout;
    hbox1->addWidget(mImageWidget);
    ui->originalGroupBox->setLayout(hbox1);

    mGenerationWidget = new GenerationWidget;
    QHBoxLayout* hbox2 = new QHBoxLayout;
    hbox2->addWidget(mGenerationWidget);
    ui->generatedGroupBox->setLayout(hbox2);

    mOptionsForm = new OptionsForm;
    mSVGViewer = new SVGViewer;

    mLogViewerForm = new LogViewerForm;
    QObject::connect(mLogViewerForm, SIGNAL(copyPicture(int, int)), SLOT(copyPicture(int, int)));
    QObject::connect(mLogViewerForm, SIGNAL(showPicture(int, int)), SLOT(showPicture(int, int)));
    QObject::connect(mLogViewerForm, SIGNAL(gotoPicture(int, int)), SLOT(gotoPicture(int, int)));

    QObject::connect(mImageWidget, SIGNAL(imageDropped(QImage)), &mBreeder, SLOT(setOriginalImage(QImage)));
    QObject::connect(mImageWidget, SIGNAL(imageDropped(QImage)), SLOT(imageDropped(QImage)));
    QObject::connect(mImageWidget, SIGNAL(colorChanged(QRgb)), mOptionsForm, SLOT(backgroundColorChanged(QRgb)));
    QObject::connect(mImageWidget, SIGNAL(colorSelected(QRgb)), mOptionsForm, SLOT(backgroundColorSelected()));
    QObject::connect(mGenerationWidget, SIGNAL(fileDropped(QString)), SLOT(loadDNA(QString)));
    QObject::connect(mGenerationWidget, SIGNAL(clickAt(const QPointF&)), &mBreeder, SLOT(spliceAt(const QPointF&)));
    QObject::connect(&mBreeder, SIGNAL(spliced(const Gene&, QVector<Gene>)), mGenerationWidget, SLOT(setSplices(const Gene&, QVector<Gene>)));

    QObject::connect(&mAutoSaveTimer, SIGNAL(timeout()), SLOT(autoSave()));

    QObject::connect(mOptionsForm, SIGNAL(autoSaveIntervalChanged(int)), SLOT(autoSaveIntervalChanged(int)));
    QObject::connect(mOptionsForm, SIGNAL(autoSaveToggled(bool)), SLOT(autoSaveToggled(bool)));
    QObject::connect(mOptionsForm, SIGNAL(startDistributionChanged()), SLOT(startDistributionChanged()));
    QObject::connect(mOptionsForm, SIGNAL(backgroundColorSelected(QRgb)), SLOT(setBackgroundColor(QRgb)));

    QObject::connect(ui->startStopPushButton, SIGNAL(clicked()), SLOT(startStop()));
    QObject::connect(ui->resetPushButton, SIGNAL(clicked()), SLOT(resetBreeder()));

    QObject::connect(ui->redSlider, SIGNAL(valueChanged(int)), &gSettings, SLOT(setDeltaR(int)));
    QObject::connect(ui->greenSlider, SIGNAL(valueChanged(int)), &gSettings, SLOT(setDeltaG(int)));
    QObject::connect(ui->blueSlider, SIGNAL(valueChanged(int)), &gSettings, SLOT(setDeltaB(int)));
    QObject::connect(ui->alphaSlider, SIGNAL(valueChanged(int)), &gSettings, SLOT(setDeltaA(int)));
    QObject::connect(ui->xySlider, SIGNAL(valueChanged(int)), &gSettings, SLOT(setDeltaXY(int)));
    QObject::connect(ui->redSlider, SIGNAL(valueChanged(int)), SLOT(setDeltaR(int)));
    QObject::connect(ui->greenSlider, SIGNAL(valueChanged(int)), SLOT(setDeltaG(int)));
    QObject::connect(ui->blueSlider, SIGNAL(valueChanged(int)), SLOT(setDeltaB(int)));
    QObject::connect(ui->alphaSlider, SIGNAL(valueChanged(int)), SLOT(setDeltaA(int)));
    QObject::connect(ui->xySlider, SIGNAL(valueChanged(int)), SLOT(setDeltaXY(int)));

    QObject::connect(ui->actionSaveDNA, SIGNAL(triggered()), SLOT(saveDNA()));
    QObject::connect(ui->actionOpenDNA, SIGNAL(triggered()), SLOT(openDNA()));
    QObject::connect(ui->actionOpenOriginalImage, SIGNAL(triggered()), SLOT(openOriginalImage()));
    QObject::connect(ui->actionSaveSettings, SIGNAL(triggered()), SLOT(saveSettings()));
    QObject::connect(ui->actionOpenSettings, SIGNAL(triggered()), SLOT(openSettings()));

    QObject::connect(ui->actionExit, SIGNAL(triggered()), SLOT(close()));
    QObject::connect(ui->actionAbout, SIGNAL(triggered()), SLOT(about()));
    QObject::connect(ui->actionAboutQt, SIGNAL(triggered()), SLOT(aboutQt()));
    QObject::connect(ui->actionOptions, SIGNAL(triggered()), mOptionsForm, SLOT(show()));
    QObject::connect(ui->actionLogViewer, SIGNAL(triggered()), mLogViewerForm, SLOT(show()));

    for (int i = 0; i < MaxRecentFiles; ++i) {
        mRecentImageFileActs[i] = new QAction(this);
        mRecentImageFileActs[i]->setVisible(false);
        QObject::connect(mRecentImageFileActs[i], SIGNAL(triggered()), SLOT(loadRecentImageFile()));
        mRecentDNAFileActs[i] = new QAction(this);
        mRecentDNAFileActs[i]->setVisible(false);
        QObject::connect(mRecentDNAFileActs[i], SIGNAL(triggered()), SLOT(loadRecentDNAFile()));
        mRecentSettingsFileActs[i] = new QAction(this);
        mRecentSettingsFileActs[i]->setVisible(false);
        QObject::connect(mRecentSettingsFileActs[i], SIGNAL(triggered()), SLOT(loadRecentSettingsFile()));
    }

    for (int i = 0; i < MaxRecentFiles; ++i) {
        ui->menuOpenRecentImage->addAction(mRecentImageFileActs[i]);
        ui->menuOpenRecentDNA->addAction(mRecentDNAFileActs[i]);
        ui->menuOpenRecentSettings->addAction(mRecentSettingsFileActs[i]);
    }

    restoreAppSettings();

    qRegisterMetaType<DNA>("DNA");
    QObject::connect(&mBreeder, SIGNAL(evolved(const QImage&, const DNA&, quint64, unsigned long, unsigned long)), SLOT(evolved(const QImage&, const DNA&, quint64, unsigned long, unsigned long)));
    QObject::connect(&mBreeder, SIGNAL(proceeded(unsigned long)), SLOT(proceeded(unsigned long)));

    const QStringList& arg = qApp->arguments();
    int idx;
    if (arg.indexOf("-no-dialogs") > 0)
        mNoDialogs = true;
    idx = arg.indexOf("-settings");
    if (idx > 0 && arg.size() > idx+1)
        loadSettings(arg.at(idx+1));

    int timeout = 0;
    idx = arg.indexOf("-timeout");
    if (idx > 0 && arg.size() > idx+1) {
        timeout = arg.at(idx+1).toInt();
        mAutoStopTimerId = startTimer(1000 * timeout);
    }
    if (arg.indexOf("-close-on-stop") > 0)
        mCloseOnStop = true;
    if (arg.indexOf("-start") > 0)
        startBreeding();

    doLog(QString("current working directory = %1").arg(QDir::currentPath()));
    if (timeout > 0)
        doLog(QString("stopping automatically after %1 seconds").arg(timeout));

#ifndef QT_NO_DEBUG
    mOptionsForm->show();
    mLogViewerForm->show();
#endif
}


MainWindow::~MainWindow()
{
    delete mLogViewerForm;
    delete mOptionsForm;
    delete mImageWidget;
    delete mGenerationWidget;
    delete ui;
}


bool MainWindow::event(QEvent* e)
{
    return QMainWindow::event(e);
}


void MainWindow::timerEvent(QTimerEvent* e)
{
    if (e->timerId() == mAutoStopTimerId) {
        doLog("AUTOSTOP.");
        mBreeder.setDirty(false);
        close();
    }
    e->accept();
}


void MainWindow::closeEvent(QCloseEvent* e)
{
    stopBreeding();
    if (mBreeder.isDirty() && !mNoDialogs) {
        QMessageBox msgBox;
        msgBox.setText(tr("<b>DNA/settings have been modified.</b>"));
        msgBox.setInformativeText(tr("You have unsaved DNA/settings. Do you want to save them?"));
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        switch (ret) {
        case QMessageBox::Save:
            autoSaveImage();
            autoSaveDNA();
            saveSettings();
            break;
        case QMessageBox::Cancel:
            e->ignore();
            return;
        case QMessageBox::Discard:
            break;
        }
    }
    saveAppSettings();
    mOptionsForm->close();
    mLogViewerForm->close();
    e->accept();
}


void MainWindow::copyPicture(int generation, int selected)
{
    const QString& imageFilename = mOptionsForm->makeImageFilename(mImageWidget->imageFileName(), generation, selected);
    const QImage img(imageFilename);
    if (img.isNull()) {
        QMessageBox::warning(this, tr("Image NOT copied"), tr("The selected image could not be copied to the clipboard. Probably you selected an image which hasn't been automatically saved."));
        return;
    }
    QApplication::clipboard()->setImage(img);
    QMessageBox::information(this, tr("Image copied to clipboard"), tr("The selected image has been copied to the clipboard."));
}


void MainWindow::showPicture(int generation, int selected)
{
    const QString& svgFilename = mOptionsForm->makeDNAFilename(mImageWidget->imageFileName(), generation, selected);
    QFileInfo fInfo(svgFilename);
    if (!fInfo.isFile() || !fInfo.isReadable()) {
        QMessageBox::warning(this, tr("DNA missing"), tr("The selected DNA could not be found or is invalid. Probably you selected a generation which hasn't been automatically saved."));
        return;
    }
    mSVGViewer->show(svgFilename, mImageWidget->image().size());
}


void MainWindow::gotoPicture(int generation, int selected)
{
    const QString& svgFilename = mOptionsForm->makeDNAFilename(mImageWidget->imageFileName(), generation, selected);
    QFileInfo fInfo(svgFilename);
    if (!fInfo.isReadable()) {
        QMessageBox::warning(this, tr("DNA not found"), tr("The selected DNA could not be found. Probably you selected a generation which hasn't been automatically saved."));
        return;
    }
    const QString& path = QString("file:///%1").arg(fInfo.absolutePath());
    const bool ok = QDesktopServices::openUrl(path);
    if (!ok) {
        QMessageBox::warning(this, tr("Directory not found"), tr("The configured save directory '%1' could not be found. Please check the corresponding form field in the options dialog.").arg(fInfo.absolutePath()), QMessageBox::Ok, QMessageBox::NoButton);
        mOptionsForm->go("Autosave", "imageSaveDirectory");
    }
}


void MainWindow::setDeltaR(int v)
{
    ui->rLineEdit->setText(QString("%1").arg(v));
}


void MainWindow::setDeltaG(int v)
{
    ui->gLineEdit->setText(QString("%1").arg(v));
}


void MainWindow::setDeltaB(int v)
{
    ui->bLineEdit->setText(QString("%1").arg(v));
}


void MainWindow::setDeltaA(int v)
{
    ui->aLineEdit->setText(QString("%1").arg(v));
}


void MainWindow::setDeltaXY(int v)
{
    ui->xyLineEdit->setText(QString("%1").arg(1e-4 * v));
}


quint64 MainWindow::totalSeconds(void) const {
    quint64 totalseconds = mBreeder.totalSeconds() + QDateTime::currentDateTime().toTime_t() - mStartTime.toTime_t();
    if (totalseconds == 0)
        totalseconds = 1;
    return totalseconds;
}


void MainWindow::proceeded(unsigned long generation)
{
    const quint64 totalseconds = totalSeconds();
    ui->gensSLineEdit->setText(QString("%1").arg((qreal)generation / totalseconds, 0, 'g', 5));
    ui->generationLineEdit->setText(QString("%1").arg(generation));
    ui->totalTimeLineEdit->setText(secondsToTime(totalseconds));
}


void MainWindow::doLog(const QString& message)
{
    if (!gSettings.logFile().isEmpty()) {
        mLog.setFileName(gSettings.logFile());
        mLog.open(QIODevice::Append | QIODevice::Text);
    }
    if (mLog.isOpen()) {
        QTextStream f(&mLog);
        f << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " "
          << message
          << "\n";
    }
}


void MainWindow::doLog(unsigned long generation, unsigned long selected, int numPoints, int numGenes, quint64 fitness, const QImage& image)
{
    if (mOptionsForm->logInternally())
        mLogViewerForm->log(generation, selected, numPoints, numGenes, fitness, image);
    if (mLog.isOpen()) {
        QTextStream f(&mLog);
        f << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << " "
          << QString("%1 %2 %3 %4 %5").arg(generation).arg(selected).arg(numPoints).arg(numGenes).arg(fitness)
          << "\n";
    }
}


void MainWindow::evolved(const QImage& image, const DNA& dna, quint64 fitness, unsigned long selected, unsigned long generation)
{
    mRecentEvolvedSelection = selected;
    mRecentEvolvedGeneration = generation;
    const int numPoints = dna.points();
    mGenerationWidget->setImage(image);
    ui->fitnessLineEdit->setText((fitness == std::numeric_limits<quint64>::max())? tr("n/a") : QString("%1%").arg(100 * (1.0 - (qreal)fitness / mBreeder.worstFitness()), 0, 'g', 9));
    ui->selectedLineEdit->setText(QString("%1").arg(selected));
    ui->selectedRatioLineEdit->setText(QString("%1%").arg(1e2 * selected / generation, 0, 'g', 5));
    ui->polygonsLineEdit->setText(QString("%1").arg(dna.size()));
    ui->pointsLineEdit->setText(QString("%1").arg(numPoints));
    mGenerationWidget->setDNA(dna);
    doLog(generation, selected, numPoints, dna.size(), fitness, image);
}


void MainWindow::evolved(void)
{
    proceeded(mBreeder.generation());
    evolved(mBreeder.image(), mBreeder.constDNA(), mBreeder.currentFitness(), mBreeder.selected(), mBreeder.selectedGeneration());
}


bool MainWindow::autoSaveImage(void)
{
    QString imageFilename = mOptionsForm->makeImageFilename(mImageWidget->imageFileName(), mRecentEvolvedGeneration, mRecentEvolvedSelection);
    avoidDuplicateFilename(imageFilename);
    return mGenerationWidget->image().save(imageFilename);
}


bool MainWindow::autoSaveDNA(void)
{
    QString dnaFilename = mOptionsForm->makeDNAFilename(mImageWidget->imageFileName(), mRecentEvolvedGeneration, mRecentEvolvedSelection);
    gSettings.setCurrentDNAFile(dnaFilename);
    DNA dna = mBreeder.dna(); // gives a clone
    return dna.save(dnaFilename, mRecentEvolvedGeneration, mRecentEvolvedSelection, mBreeder.currentFitness(), totalSeconds());
}


void MainWindow::autoSave(void)
{
    const QCursor oldCursor = cursor();
    setCursor(Qt::WaitCursor);
    bool success = autoSaveImage() && autoSaveDNA();
    if (success) {
        statusBar()->showMessage(tr("Automatically saved mutation %1 out of %2 generations.").arg(mRecentEvolvedSelection).arg(mRecentEvolvedGeneration), 3000);
        mLogViewerForm->highlightLastRow();
    }
    else {
        statusBar()->showMessage(tr("Automatic saving failed."), 3000);
    }
    setCursor(oldCursor);
    if (mOptionsForm->stopOnNextAutosave() && sender() /* only stop if called from slot */) {
        mOptionsForm->setStopOnNextAutosave(false);
        stopBreeding();
    }
}


void MainWindow::startDistributionChanged(void)
{
    if (!mBreeder.isRunning())
        resetBreeder();
}


void MainWindow::autoSaveIntervalChanged(int interval)
{
    if (mOptionsForm->autoSave() && interval > 0) {
        const bool wasActive = mAutoSaveTimer.isActive();
        mAutoSaveTimer.stop();
        mAutoSaveTimer.setInterval(1000 * interval);
        if (wasActive)
            mAutoSaveTimer.start();
    }
}


void MainWindow::autoSaveToggled(bool enabled)
{
    if (enabled) {
        const bool wasActive = mAutoSaveTimer.isActive();
        mAutoSaveTimer.setInterval(1000 * mOptionsForm->saveInterval());
        if (wasActive)
            mAutoSaveTimer.start();
    }
    else {
        mAutoSaveTimer.stop();
    }
}


void MainWindow::setBackgroundColor(QRgb)
{
    mBreeder.generate();
    evolved();
}


void MainWindow::startBreeding(void)
{
    QFileInfo info;
    // check if DNA save directory exists
    info.setFile(mOptionsForm->dnaSaveDirectory());
    if (!info.exists() || !info.isWritable() || !info.isDir()) {
        QMessageBox::warning(this, tr("DNA save directory missing"), tr("The selected DNA save directory '%1' (%2) does not exist. Please go to the options dialog and choose a new one. Then try starting again.").arg(mOptionsForm->dnaSaveDirectory()).arg(info.absoluteFilePath()));
        mOptionsForm->go("Autosave", "dnaSaveDirectory");
        return;
    }
    // check if image save directory exists
    info.setFile(mOptionsForm->imageSaveDirectory());
    if (!info.exists() || !info.isWritable() || !info.isDir()) {
        QMessageBox::warning(this, tr("Image save directory missing"), tr("The selected image save directory '%1' does not exist. Please go to the options dialog and choose a new one. Then try starting again.").arg(mOptionsForm->imageSaveDirectory()).arg(info.absoluteFilePath()));
        mOptionsForm->go("Autosave", "imageSaveDirectory");
        return;
    }
    // check if log file is writable
    if (!mOptionsForm->logFile().isEmpty()) {
        info.setFile(mOptionsForm->logFile());
        QFileInfo dirInfo(info.absolutePath());
        if (!dirInfo.isDir() || !dirInfo.isWritable()) {
            QMessageBox::warning(this, tr("Log file is not writable"), tr("The selected log file is not writable. Please go to the options dialog and choose a new one. Then try starting again."));
            mOptionsForm->go("Autosave", "logFile");
            return;
        }
    }

    statusBar()->showMessage(tr("Starting ..."), 3000);
    if (!mOptionsForm->logFile().isEmpty() && !mLog.isOpen()) {
        mLog.setFileName(mOptionsForm->logFile());
        mLog.open(QIODevice::Append | QIODevice::Text);
        doLog("START.");
    }
    mStartTime = QDateTime::currentDateTime();
    mBreeder.breed();
    if (mOptionsForm->autoSave()) {
        mAutoSaveTimer.setInterval(1000 * mOptionsForm->saveInterval());
        mAutoSaveTimer.start();
        if (ui->startStopPushButton->text() == tr("Start"))
            autoSave();
    }
    ui->startStopPushButton->setText(tr("Pause"));
}


void MainWindow::stopBreeding(void)
{
    if (!mBreeder.isRunning())
        return;
    statusBar()->showMessage(tr("Stopping ..."), 550);
    ui->startStopPushButton->setText(tr("Resume"));
    mAutoSaveTimer.stop();
    mBreeder.stop();
    mBreeder.addTotalSeconds(QDateTime::currentDateTime().toTime_t() - mStartTime.toTime_t());
    doLog("STOP.");
    if (mLog.isOpen())
        mLog.close();
}


void MainWindow::startStop(void)
{
    if (ui->startStopPushButton->text() == tr("Start") || ui->startStopPushButton->text() == tr("Resume")) {
        startBreeding();
    }
    else {
        stopBreeding();
    }
}


void MainWindow::saveAppSettings(void)
{
    QSettings settings(Company, AppName);
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/windowState", saveState());
    settings.setValue("MainWindow/imageFilename", gSettings.currentImageFile());
    settings.setValue("MainWindow/dnaFilename", gSettings.currentDNAFile());
    settings.setValue("LogViewer/geometry", mLogViewerForm->saveGeometry());
    settings.setValue("LogViewer/visible", mLogViewerForm->isVisible());
    settings.setValue("Options/visible", mOptionsForm->isVisible());
    settings.setValue("Options/geometry", mOptionsForm->saveGeometry());
    settings.setValue("Options/deltaR", ui->redSlider->value());
    settings.setValue("Options/deltaG", ui->greenSlider->value());
    settings.setValue("Options/deltaB", ui->blueSlider->value());
    settings.setValue("Options/deltaA", ui->alphaSlider->value());
    settings.setValue("Options/deltaXY", ui->xySlider->value());
    settings.setValue("Options/colorMutationProbability", mOptionsForm->colorMutationProbability());
    settings.setValue("Options/pointMutationProbability", mOptionsForm->pointMutationProbability());
    settings.setValue("Options/pointKillProbability", mOptionsForm->pointKillProbability());
    settings.setValue("Options/pointEmergenceProbability", mOptionsForm->pointEmergenceProbability());
    settings.setValue("Options/geneKillProbability", mOptionsForm->geneKillProbability());
    settings.setValue("Options/geneMoveProbability", mOptionsForm->geneMoveProbability());
    settings.setValue("Options/geneEmergenceProbability", mOptionsForm->geneEmergenceProbability());
    settings.setValue("Options/minPointsPerGene", mOptionsForm->minPointsPerGene());
    settings.setValue("Options/maxPointsPerGene", mOptionsForm->maxPointsPerGene());
    settings.setValue("Options/minGenes", mOptionsForm->minGenes());
    settings.setValue("Options/maxGenes", mOptionsForm->maxGenes());
    settings.setValue("Options/minAlpha", mOptionsForm->minAlpha());
    settings.setValue("Options/maxAlpha", mOptionsForm->maxAlpha());
    settings.setValue("Options/imageSaveDirectory", mOptionsForm->imageSaveDirectory());
    settings.setValue("Options/imageSaveFilenameTemplate", mOptionsForm->imageSaveFilenameTemplate());
    settings.setValue("Options/dnaSaveDirectory", mOptionsForm->dnaSaveDirectory());
    settings.setValue("Options/dnaSaveFilenameTemplate", mOptionsForm->dnaSaveFilenameTemplate());
    settings.setValue("Options/logFile", mOptionsForm->logFile());
    settings.setValue("Options/internalLogEnabled", mOptionsForm->logInternally());
    settings.setValue("Options/saveInterval", mOptionsForm->saveInterval());
    settings.setValue("Options/autoSave", mOptionsForm->autoSave());
    settings.setValue("Options/startDistribution", mOptionsForm->startDistribution());
    settings.setValue("Options/scatterFactor", mOptionsForm->scatterFactor());
    settings.setValue("Options/cores", mOptionsForm->cores());
}


void MainWindow::restoreAppSettings(void)
{
    QSettings settings(Company, AppName);
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    restoreState(settings.value("MainWindow/windowState").toByteArray());
    ui->redSlider->setValue(settings.value("Options/deltaR", 50).toInt());
    ui->greenSlider->setValue(settings.value("Options/deltaG", 50).toInt());
    ui->blueSlider->setValue(settings.value("Options/deltaB", 50).toInt());
    ui->alphaSlider->setValue(settings.value("Options/deltaA", 50).toInt());
    ui->xySlider->setValue(settings.value("Options/deltaXY", 2300).toInt());
    mLogViewerForm->restoreGeometry(settings.value("LogViewer/geometry").toByteArray());
    mOptionsForm->restoreGeometry(settings.value("Options/geometry").toByteArray());
    mOptionsForm->setImageSaveDirectory(settings.value("Options/imageSaveDirectory", QDir::homePath()).toString());
    mOptionsForm->setImageSaveFilenameTemplate(settings.value("Options/imageSaveFilenameTemplate", "%1-%2-%3.png").toString());
    mOptionsForm->setDNASaveDirectory(settings.value("Options/dnaSaveDirectory", QDir::homePath()).toString());
    mOptionsForm->setDNASaveFilenameTemplate(settings.value("Options/dnaSaveFilenameTemplate", "%1-%2-%3.svg").toString());
    mOptionsForm->setSaveInterval(settings.value("Options/saveInterval", 10).toInt());
    mOptionsForm->setAutoSave(settings.value("Options/autoSave", true).toBool());
    mOptionsForm->setLogFile(settings.value("Options/logFile").toString());
    mOptionsForm->setInternalLogEnabled(settings.value("Options/internalLogEnabled", false).toBool());
    mOptionsForm->setCores(settings.value("Options/cores", QThread::idealThreadCount()).toInt());
    mOptionsForm->setStartDistribution(settings.value("Options/startDistribution", 4).toInt());
    mOptionsForm->setScatterFactor(settings.value("Options/scatterFactor", 0.45).toDouble());
    mOptionsForm->setColorMutationProbability(settings.value("Options/colorMutationProbability", 1000).toInt());
    mOptionsForm->setPointMutationProbability(settings.value("Options/pointMutationProbability", 1000).toInt());
    mOptionsForm->setPointKillProbability(settings.value("Options/pointKillProbability", 1000).toInt());
    mOptionsForm->setPointEmergenceProbability(settings.value("Options/pointEmergenceProbability", 1000).toInt());
    mOptionsForm->setGeneKillProbability(settings.value("Options/geneKillProbability", 1000).toInt());
    mOptionsForm->setGeneMoveProbability(settings.value("Options/geneMoveProbability", 5000).toInt());
    mOptionsForm->setGeneEmergenceProbability(settings.value("Options/geneEmergenceProbability", 1000).toInt());
    mOptionsForm->setMinPointsPerGene(settings.value("Options/minPointsPerGene", 3).toInt());
    mOptionsForm->setMaxPointsPerGene(settings.value("Options/maxPointsPerGene", 5).toInt());
    mOptionsForm->setMinGenes(settings.value("Options/minGenes", 150).toInt());
    mOptionsForm->setMaxGenes(settings.value("Options/maxGenes", 500).toInt());
    mOptionsForm->setMinAlpha(settings.value("Options/minAlpha", 5).toInt());
    mOptionsForm->setMaxAlpha(settings.value("Options/maxAlpha", 45).toInt());
    if (settings.value("Options/visible").toBool())
        mOptionsForm->show();
    if (settings.value("LogViewer/visible").toBool())
        mLogViewerForm->show();
    QString imageFileName = settings.value("MainWindow/imageFilename", ":/images/KWA10.jpg").toString();
    mImageWidget->loadImage(imageFileName);
    loadDNA(settings.value("MainWindow/dnaFilename").toString());
    updateRecentFileActions("Options/recentImageFileList", ui->menuOpenRecentImage, mRecentImageFileActs);
    updateRecentFileActions("Options/recentDNAFileList", ui->menuOpenRecentDNA, mRecentDNAFileActs);
    updateRecentFileActions("Options/recentSettingsFileList", ui->menuOpenRecentSettings, mRecentSettingsFileActs);
}


void MainWindow::saveSettings(void)
{
    const QString& settingsFilename = QFileDialog::getSaveFileName(this, tr("Save settings"), QString(), tr("Settings file (*.evo; *.xml)"));
    if (settingsFilename.isNull())
        return;
    appendToRecentFileList(settingsFilename, "Options/recentSettingsFileList");
    gSettings.save(settingsFilename);
}


void MainWindow::saveDNA(void)
{
    QString dnaFilename = QFileDialog::getSaveFileName(this, tr("Save DNA"), QString(), tr("DNA files (*.svg; *.json; *.dna)"));
    if (dnaFilename.isNull())
        return;
    DNA dna = mBreeder.dna();
    bool success = dna.save(dnaFilename, mBreeder.generation(), mBreeder.selected(), mBreeder.currentFitness(), totalSeconds());
    if (success) {
        statusBar()->showMessage(tr("DNA saved as '%1'.").arg(dnaFilename), 5000);
    }
    else {
        QMessageBox::warning(this, tr("Error saving DNA"), tr("DNA could not be saved as '%1'.").arg(dnaFilename));
    }
    mBreeder.setDirty(false);
}


void MainWindow::imageDropped(const QImage&)
{
    evolved(mBreeder.image(), mBreeder.constDNA(), mBreeder.currentFitness(), mBreeder.selected(), mBreeder.generation());
}


void MainWindow::openOriginalImage(void)
{
    const QString& filename = QFileDialog::getOpenFileName(this, tr("Load original picture"));
    loadOriginalImage(filename);
}


void MainWindow::loadOriginalImage(const QString& filename)
{
    if (!filename.isEmpty()) {
        const bool success = mImageWidget->loadImage(filename);
        if (success) {
            statusBar()->showMessage(tr("Original picture '%1' loaded.").arg(filename), 3000);
            doLog(tr("Original picture '%1' loaded.").arg(filename));
            appendToRecentFileList(filename, "Options/recentImageFileList");
            updateRecentFileActions("Options/recentImageFileList", ui->menuOpenRecentImage, mRecentImageFileActs);
            gSettings.setCurrentImageFile(filename);
        }
        else {
            doLog(tr("Original picture ('%1') could not be loaded.").arg(filename));
            QImage img(filename);
            doLog(QString("Image size: %1x%2").arg(img.width()).arg(img.height()));
            if (img.isNull())
                doLog(tr("SECOND TRY: Original picture ('%1') could not be loaded. ").arg(filename));
            if (!mNoDialogs)
                QMessageBox::warning(this, tr("Error loading the original picture."), tr("Original picture ('%1') could not be loaded.").arg(filename));
        }
    }
}


void MainWindow::loadSettings(const QString& filename)
{
    if (!filename.isEmpty()) {
        const bool success = gSettings.load(filename);
        if (success) {
            stopBreeding();
            loadOriginalImage(gSettings.currentImageFile());
            loadDNA(gSettings.currentDNAFile());
            appendToRecentFileList(filename, "Options/recentSettingsFileList");
            updateRecentFileActions("Options/recentSettingsFileList", ui->menuOpenRecentSettings, mRecentSettingsFileActs);
            ui->redSlider->setValue(gSettings.dR());
            ui->greenSlider->setValue(gSettings.dG());
            ui->blueSlider->setValue(gSettings.dB());
            ui->alphaSlider->setValue(gSettings.dA());
            ui->xySlider->setValue(1e4 * gSettings.dXY());
            mOptionsForm->setImageSaveDirectory(gSettings.imageSaveDirectory());
            mOptionsForm->setImageSaveFilenameTemplate(gSettings.imageSaveFilenameTemplate());
            mOptionsForm->setDNASaveDirectory(gSettings.dnaSaveDirectory());
            mOptionsForm->setDNASaveFilenameTemplate(gSettings.dnaSaveFilenameTemplate());
            mOptionsForm->setSaveInterval(gSettings.autoSaveInterval());
            mOptionsForm->setAutoSave(gSettings.autoSave());
            mOptionsForm->setLogFile(gSettings.logFile());
            mOptionsForm->setCores(gSettings.cores());
            mOptionsForm->setStartDistribution(gSettings.startDistribution());
            mOptionsForm->setScatterFactor(gSettings.scatterFactor());
            mOptionsForm->setColorMutationProbability(gSettings.colorMutationProbability());
            mOptionsForm->setPointMutationProbability(gSettings.pointMutationProbability());
            mOptionsForm->setPointKillProbability(gSettings.pointKillProbability());
            mOptionsForm->setPointEmergenceProbability(gSettings.pointEmergenceProbability());
            mOptionsForm->setGeneKillProbability(gSettings.geneKillProbability());
            mOptionsForm->setGeneMoveProbability(gSettings.geneMoveProbability());
            mOptionsForm->setGeneEmergenceProbability(gSettings.geneEmergenceProbability());
            mOptionsForm->setMinPointsPerGene(gSettings.minPointsPerGene());
            mOptionsForm->setMaxPointsPerGene(gSettings.maxPointsPerGene());
            mOptionsForm->setMinGenes(gSettings.minGenes());
            mOptionsForm->setMaxGenes(gSettings.maxGenes());
            mOptionsForm->setMinAlpha(gSettings.minA());
            mOptionsForm->setMaxAlpha(gSettings.maxA());
            mOptionsForm->setStopOnNextAutosave(gSettings.stopOnAutoSave());
            statusBar()->showMessage(tr("Settings file '%1' loaded.").arg(filename), 3000);
            doLog(tr("Settings file '%1' loaded.").arg(filename));
        }
        else {
            QMessageBox::warning(this, tr("Error loading settings"), tr("Settings could not be loaded. (%1)").arg(gSettings.errorString()));
        }
    }
}


void MainWindow::openSettings(void)
{
    const QString& filename = QFileDialog::getOpenFileName(this, tr("Load Settings"), QString(), tr("Settings files (*.evo; *.xml)"));
    loadSettings(filename);
}


void MainWindow::loadDNA(const QString& filename)
{
    if (!filename.isEmpty()) {
        DNA dna;
        const bool success = dna.load(filename);
        if (success) {
            stopBreeding();
            mBreeder.setDNA(dna);
            if (dna.generation() == 0 || dna.selected() == 0) { // evaluation of metadata section failed
                QRegExp re("(\\d{10}).+(\\d{9})");
                if (re.indexIn(filename) != -1) {
                    const QStringList& n = re.capturedTexts();
                    mBreeder.setGeneration(n.at(1).toULong());
                    mBreeder.setSelected(n.at(2).toULong());
                }
                else {
                    qWarning() << "MainWindow::loadDNA(" << filename << ") Cannot determine generation and selected from filename or metadata. Tried metadata first.";
                }
            }
            mStartTime = QDateTime::currentDateTime();
            proceeded(mBreeder.generation());
            evolved(mBreeder.image(), mBreeder.constDNA(), mBreeder.currentFitness(), mBreeder.selected(), mBreeder.generation());
            statusBar()->showMessage(tr("DNA '%1' loaded.").arg(filename), 3000);
            appendToRecentFileList(filename, "Options/recentDNAFileList");
            updateRecentFileActions("Options/recentDNAFileList", ui->menuOpenRecentDNA, mRecentDNAFileActs);
            gSettings.setCurrentDNAFile(filename);
        }
        else {
            QMessageBox::warning(this, tr("Error loading DNA"), tr("DNA could not be loaded. (%1)").arg(dna.errorString()));
        }
    }
}


void MainWindow::openDNA(void)
{
    const QString& filename = QFileDialog::getOpenFileName(this, tr("Load DNA"), QString(), tr("DNA files (*.svg *.json *.dna)"));
    loadDNA(filename);
}


void MainWindow::loadRecentDNAFile(void)
{
    const QAction* const action = qobject_cast<QAction*>(sender());
    if (action)
        loadDNA(action->data().toString());
}


void MainWindow::loadRecentImageFile(void)
{
    const QAction* const action = qobject_cast<QAction*>(sender());
    if (action)
        loadOriginalImage(action->data().toString());
}



void MainWindow::loadRecentSettingsFile(void)
{
    const QAction* const action = qobject_cast<QAction*>(sender());
    if (action)
        loadSettings(action->data().toString());
}


void MainWindow::appendToRecentFileList(const QString& fileName, const QString& listName)
{
    QSettings settings(Company, AppName);
    QStringList files = settings.value(listName).toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > MaxRecentFiles)
        files.removeLast();
    settings.setValue(listName, files);
}


QString MainWindow::mostRecentFileInList(const QString& listName)
{
    QSettings settings(Company, AppName);
    QStringList files = settings.value(listName).toStringList();
    return files.first();
}


void MainWindow::updateRecentFileActions(const QString& listName, QMenu* menu, QAction* actions[])
{
    QSettings settings(Company, AppName);
    const QStringList& files = settings.value(listName).toStringList();
    QStringList updatedFiles;
    QStringList::const_iterator file = files.constBegin();
    while (file != files.constEnd() && updatedFiles.size() < MaxRecentFiles) {
        QFileInfo fInfo(*file);
        // only keep readable files, remove duplicates
        if (!updatedFiles.contains(*file) && fInfo.isFile() && fInfo.isReadable()) {
            const int i = updatedFiles.size();
            const QString& text = tr("&%1 %2").arg(i).arg(fInfo.fileName());
            actions[i]->setText(text);
            actions[i]->setData(*file);
            actions[i]->setVisible(true);
            updatedFiles.append(*file);
        }
        ++file;
    }
    for (int i = updatedFiles.size(); i < MaxRecentFiles; ++i)
        actions[i]->setVisible(false);
    menu->setEnabled(updatedFiles.size() > 0);
    settings.setValue(listName, updatedFiles);
}


void MainWindow::resetBreeder(void)
{
    bool ok = true;
    if (mBreeder.isDirty())
        ok = QMessageBox::question(this, tr("Really reset breeder?"), tr("Do you really want to reset the breeder?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes;
    if (ok) {
        stopBreeding();
        mStartTime = QDateTime::currentDateTime();
        mBreeder.reset();
        ui->startStopPushButton->setText(tr("Start"));
    }
}


void MainWindow::about(void)
{
    QMessageBox::about(this, tr("About %1 %2%3").arg(AppName).arg(AppVersionNoDebug).arg(AppMinorVersion),
                       tr("<p><b>%1</b> calculates vector images from bitmaps by using genetic algorithms. "
                          "See <a href=\"%2\" title=\"%1 project homepage\">%2</a> for more info.</p>"
                          "<p>Copyright &copy; 2012 %3 &lt;%4&gt;</p>"
                          "<p>This program is free software: you can redistribute it and/or modify "
                          "it under the terms of the GNU General Public License as published by "
                          "the Free Software Foundation, either version 3 of the License, or "
                          "(at your option) any later version.</p>"
                          "<p>This program is distributed in the hope that it will be useful, "
                          "but WITHOUT ANY WARRANTY; without even the implied warranty of "
                          "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the "
                          "GNU General Public License for more details.</p>"
                          "You should have received a copy of the GNU General Public License "
                          "along with this program. "
                          "If not, see <a href=\"http://www.gnu.org/licenses/gpl-3.0\">http://www.gnu.org/licenses</a>.</p>")
                       .arg(AppName).arg(AppUrl).arg(AppAuthor).arg(AppAuthorMail));
}


void MainWindow::aboutQt(void)
{
    QMessageBox::aboutQt(this);
}
