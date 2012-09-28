// Copyright (c) 2012 Oliver Lau <oliver@von-und-fuer-lau.de>
// All rights reserved.

#ifndef __MAINWINDOW_H_
#define __MAINWINDOW_H_

#include <QMainWindow>
#include <QDateTime>
#include <QString>
#include <QTimer>
#include <QFile>
#include <QEvent>
#include <QAction>
#include <QtCore/QDebug>

#include "imagewidget.h"
#include "generationwidget.h"
#include "breeder.h"
#include "optionsform.h"


namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = NULL);
    ~MainWindow();

protected:
    bool event(QEvent*);
    void closeEvent(QCloseEvent*);

private:
    Ui::MainWindow *ui;
    OptionsForm* mOptionsForm;
    ImageWidget* mImageWidget;
    GenerationWidget* mGenerationWidget;
    Breeder mBreeder;
    QDateTime mStartTime;
    QFile mLog;

    static const int MaxRecentFiles = 10;
    QAction* mRecentImageFileActs[MaxRecentFiles];
    QAction* mRecentDNAFileActs[MaxRecentFiles];
    QAction* mRecentSettingsFileActs[MaxRecentFiles];

    void saveAppSettings(void);
    void restoreAppSettings(void);
    void startBreeding(void);
    void stopBreeding(void);
    void loadOriginalImage(const QString& filename);
    quint64 totalSeconds(void) const;
    void appendToRecentFileList(const QString& fileName, const QString& listName);
    QString mostRecentFileInList(const QString&);

    QTimer mAutoSaveTimer;

private slots:
    void loadSettings(const QString& filename);
    void loadDNA(const QString& filename);
    void evolved(const QImage&, const DNA&, quint64 fitness, unsigned long selected, unsigned long generation);
    void imageDropped(const QImage&);
    void proceeded(unsigned long generation);
    void startStop(void);
    void openSettings(void);
    void saveSettings(void);
    void saveDNA(void);
    void openDNA(void);
    void openOriginalImage(void);
    void resetBreeder(void);
    void about(void);
    void aboutQt(void);
    void autoSaveGeneratedImage(void);
    void autoSaveIntervalChanged(int);
    void autoSaveToggled(bool);
    void loadRecentImageFile(void);
    void loadRecentDNAFile(void);
    void loadRecentSettingsFile(void);
    void updateRecentFileActions(const QString& listName, QMenu* menu, QAction* actions[]);

    void setDeltaR(int);
    void setDeltaG(int);
    void setDeltaB(int);
    void setDeltaA(int);
    void setDeltaXY(int);
};

#endif // __MAINWINDOW_H_
