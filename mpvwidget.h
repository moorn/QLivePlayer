#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H

#include <QtWidgets/QOpenGLWidget>
#include <mpv/client.h>
#include <mpv/opengl_cb.h>
#include <mpv/qthelper.hpp>
#include <QPainter>
#include <QFontMetrics>
#include <QFont>
#include <QLabel>
#include <QPalette>
#include <QPropertyAnimation>
#include <QList>
#include <QStringList>
#include <QString>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <QtGlobal>
#include <QFile>
#include <QTextStream>
#include <QIODevice>
#include <cmath>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QMainWindow>
#include <QCoreApplication>
#include <QApplication>

class MpvWidget : public QOpenGLWidget
{
    Q_OBJECT
public:
    MpvWidget(QWidget *parent = 0, Qt::WindowFlags f = 0);
    ~MpvWidget();
    void command(const QVariant& params);
    void setProperty(const QString& name, const QVariant& value);
    QVariant getProperty(const QString& name) const;
    QSize sizeHint() const { return QSize(1280, 720);}

Q_SIGNALS:
    void durationChanged(int value);
    void positionChanged(int value);
protected:
    void initializeGL() Q_DECL_OVERRIDE;
    void paintGL() Q_DECL_OVERRIDE;
private Q_SLOTS:
    void swapped();
    void on_mpv_events();
    void maybeUpdate();
private:
    void handle_mpv_event(mpv_event *event);
    static void on_update(void *ctx);

    mpv::qt::Handle mpv;
    mpv_opengl_cb_context *mpv_gl;

};

class DanmakuPlayer : public MpvWidget
{
    Q_OBJECT
public:
    DanmakuPlayer(QWidget *parent = 0, Qt::WindowFlags f = 0);
    ~DanmakuPlayer();

    void initDanmaku();
    bool isDanmakuVisible();
    void launchDanmaku(QString danmakuText);
    int getAvailDanmakuChannel();
    void setRandomSequence(int baseIndex, int lengh);
    void updateDanmakuFrequency();

protected:
    void keyPressEvent(QKeyEvent *event);

signals:
    void closeDanmaku();

private:
    bool danmakuHighFreqMode = false;
    int danmakuFrequency[4] = {0};
    int danmakuChannelIndex = 0;
    int danmakuChannelSequence[24] = {0};
    quint32 danmakuChannelMask = 0x0000FFFF;
    bool danmakuShowFlag = true;
};


#endif // PLAYERWINDOW_H
