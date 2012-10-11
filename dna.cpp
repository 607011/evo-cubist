// Copyright (c) 2012 Oliver Lau <oliver@von-und-fuer-lau.de>
// All rights reserved.

#include <QDateTime>
#include <QIODevice>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QTextCodec>
#include <QtCore/QDebug>

#include "qt-json/json.h"
#include "gene.h"
#include "dna.h"
#include "svgreader.h"
#include "breedersettings.h"
#include "main.h"
#include "helper.h"
#include "random/rnd.h"

using namespace QtJson;


/// deep copy
DNA::DNA(const DNA& other)
    : mSize(other.mSize)
    , mGeneration(other.mGeneration)
    , mSelected(other.mSelected)
    , mFitness(other.mFitness)
    , mTotalSeconds(other.mTotalSeconds)
{
    mDNA.reserve(other.mDNA.size());
    for (DNAType::const_iterator gene = other.mDNA.constBegin(); gene != other.mDNA.constEnd(); ++gene)
        mDNA.append(*gene);
}


inline bool DNA::willMutate(unsigned int probability) {
    return RAND::rnd(probability) == 0;
}


QPolygonF DNA::findPolygonForPoint(const QPointF& p) const
{
    int i = mDNA.size();
    while (i--) {
        const Gene& gene = mDNA.at(i);
        if (gene.polygon().containsPoint(p, Qt::WindingFill))
            return gene.polygon();
    }
    return QPolygonF();
}


void DNA::mutate(void)
{
    // maybe spawn a new gene
    if (willMutate(gSettings.geneEmergenceProbability()) && mDNA.size() < gSettings.maxGenes())
        mDNA.append(Gene(true));
    // maybe kill a gene
    if (willMutate(gSettings.geneKillProbability()) && mDNA.size() > gSettings.minGenes())
        mDNA.remove(RAND::rnd(mDNA.size()));
    if (willMutate(gSettings.geneMoveProbability())) {
        const int oldIndex = RAND::rnd(mDNA.size());
        const int newIndex = RAND::rnd(mDNA.size());
        if (oldIndex != newIndex) {
            const Gene gene = mDNA.at(oldIndex);
            mDNA.remove(oldIndex);
            mDNA.insert(newIndex, gene);
        }
    }
    // mutate all contained genes
    for (DNAType::iterator gene = mDNA.begin(); gene != mDNA.end(); ++gene)
        gene->mutate();
}


bool DNA::save(QString& filename, unsigned long generation, unsigned long selected, quint64 fitness, quint64 totalSeconds)
{
    bool rc;
    avoidDuplicateFilename(filename);
    QFile file(filename);
    rc = file.open(QIODevice::WriteOnly);
    if (!rc)
        return false;
    QTextStream out(&file);
    out.setAutoDetectUnicode(false);
    out.setCodec(QTextCodec::codecForMib(106/* UTF-8 */));
    if (filename.endsWith(".json") || filename.endsWith(".dna")) {
        out << "{\n"
            << " \"datetime\": \"" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz") << "\",\n"
            << " \"totalseconds\": \"" << totalSeconds << "\",\n"
            << " \"totaltime\": \"" << secondsToTime(totalSeconds) << "\",\n"
            << " \"generation\": " << generation << ",\n"
            << " \"selected\": " << selected << ",\n"
            << " \"fitness\": " << fitness << ",\n"
            << " \"deltared\": " << gSettings.dR() << ",\n"
            << " \"deltagreen\": " << gSettings.dG() << ",\n"
            << " \"deltablue\": " << gSettings.dB() << ",\n"
            << " \"deltaalpha\": " << gSettings.dA() << ",\n"
            << " \"deltaxy\": " << gSettings.dXY() << ",\n"
            << " \"size\": { \"width\": " << mSize.width() << ", \"height\": " << mSize.height() << " },\n"
            << " \"dna\": [\n";
        for (DNAType::const_iterator gene = mDNA.constBegin(); gene != mDNA.constEnd(); ++gene) {
            out << *gene;
            if ((gene+1) != mDNA.constEnd())
                out << ",";
            out << "\n";
        }
        out << "] }\n";
    }
    else if (filename.endsWith(".svg")) {
        out << "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
            << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"" << mSize.width() << "\" height=\"" << mSize.height() << "\">\n"
            << "<title>Mutation " << selected << " out of " << generation << "</title>\n"
            << "<desc>Generated by evo-cubist. Copyright (c) 2012 Oliver Lau</desc>\n"
            << "<desc xmlns:evocubist=\"http://von-und-fuer-lau.de/evo-cubist\" version=\"" << AppVersionNoDebug << "\">\n"
            << " <evocubist:totalseconds>" << totalSeconds << "</evocubist:totalseconds>\n"
            << " <evocubist:totaltime>" << secondsToTime(totalSeconds) << "</evocubist:totaltime>\n"
            << " <evocubist:generation>" << generation << "</evocubist:generation>\n"
            << " <evocubist:selected>" << selected << "</evocubist:selected>\n"
            << " <evocubist:fitness>" << fitness << "</evocubist:fitness>\n"
            << " <evocubist:deltared>" << gSettings.dR() << "</evocubist:deltared>\n"
            << " <evocubist:deltagreen>" << gSettings.dG() << "</evocubist:deltagreen>\n"
            << " <evocubist:deltablue>" << gSettings.dB() << "</evocubist:deltablue>\n"
            << " <evocubist:deltaalpha>" << gSettings.dA() << "</evocubist:deltaalpha>\n"
            << " <evocubist:deltaxy>" << gSettings.dXY() << "</evocubist:deltaxy>\n"
            << "</desc>\n"
            << " <g transform=\"scale(" << mSize.width() << ", " << mSize.height() << ")\">\n";
        for (DNAType::const_iterator gene = mDNA.constBegin(); gene != mDNA.constEnd(); ++gene) {
            if (gene->polygon().isEmpty()) // just in case
                continue;
            const QColor& c = gene->color();
            out << "  <path style=\""
                << "fill-opacity:" << c.alphaF() << ";"
                << "fill:rgb(" << c.red() << "," << c.green() << "," << c.blue() << ");" << "\""
                << " d=\"M " << gene->polygon().first().x() << " " << gene->polygon().first().y();
            for (QPolygonF::const_iterator p = gene->polygon().constBegin() + 1; p != gene->polygon().constEnd(); ++p)
                out << " L " << p->x() << " " << p->y();
            out << " Z\" />\n";
        }
        out << " </g>\n"
            << "</svg>\n";
    }
    else {
        qWarning() << "DNA::save() unknown format";
    }
    file.close();
    return rc;
}


// XXX: move method to Breeder
bool DNA::load(const QString& filename)
{
    QFile file(filename);
    bool success = file.open(QIODevice::ReadOnly);
    if (!success) {
        mErrorString = file.errorString();
        return false;
    }
    QTextStream in(&file);
    success = false;
    if (filename.endsWith(".json") || filename.endsWith(".dna")) {
        QString jsonDNA = in.readAll();
        const QVariant& v = Json::parse(jsonDNA, success);
        if (!success) {
            mErrorString = "JSON data parse error";
            return false;
        }
        const QVariantMap& result = v.toMap();
        clear();
        const QVariantMap& size = result["size"].toMap();
        mSize = QSize(size["width"].toInt(), size["height"].toInt());
        mTotalSeconds = result["datetime"].toString().toULongLong();
        mGeneration = result["generation"].toString().toULong();
        mSelected = result["selected"].toString().toULong();
        mFitness = result["fitness"].toString().toULongLong();
        const QVariantList& dna = result["dna"].toList();
        for (QVariantList::const_iterator gene = dna.constBegin(); gene != dna.constEnd(); ++gene) {
            const QVariantMap& g = gene->toMap();
            const QVariantMap& rgb = g["color"].toMap();
            QColor color(rgb["r"].toInt(), rgb["g"].toInt(), rgb["b"].toInt());
            color.setAlphaF(rgb["a"].toDouble());
            const QVariantList& vertices = g["vertices"].toList();
            QPolygonF polygon;
            for (QVariantList::const_iterator point = vertices.constBegin(); point != vertices.constEnd(); ++point) {
                const QVariantMap& p = point->toMap();
                polygon << QPointF(p["x"].toDouble(), p["y"].toDouble());
            }
            mDNA.append(Gene(polygon, color));
        }
    }
    else if (filename.endsWith(".svg")) {
        SVGReader xml;
        success = xml.readSVG(&file);
        if (!success) {
            mErrorString = xml.errorString();
            return false;
        }
        clear();
        *this = xml.dna();
    }
    else {
        mErrorString = "unknown file format";
        return false;
    }
    file.close();
    return true;
}


/// summarize number of points in all contained genes
unsigned int DNA::points(void) const
{
    unsigned int sum = 0;
    for (DNAType::const_iterator gene = mDNA.constBegin(); gene != mDNA.constEnd(); ++gene)
        sum += gene->polygon().size();
    return sum;
}
