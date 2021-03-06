#include "mpvwidget.h"
#include <stdexcept>
#include <QtGui/QOpenGLContext>
#include <QtCore/QMetaObject>
#include <QFontMetrics>
#include <QPainterPath>

static void wakeup(void *ctx)
{
    QMetaObject::invokeMethod((MpvWidget*)ctx, "on_mpv_events", Qt::QueuedConnection);
}

static void *get_proc_address(void *ctx, const char *name) {
    Q_UNUSED(ctx);
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx)
        return NULL;
    return (void *)glctx->getProcAddress(QByteArray(name));
}

MpvWidget::MpvWidget(QWidget *parent, Qt::WindowFlags f)
    : QOpenGLWidget(parent, f)
{
    mpv = mpv::qt::Handle::FromRawHandle(mpv_create());
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    mpv_set_option_string(mpv, "terminal", "yes");
    //    mpv_set_option_string(mpv, "msg-level", "all=v");
    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");

    // Make use of the MPV_SUB_API_OPENGL_CB API.
    mpv::qt::set_option_variant(mpv, "vo", "opengl-cb");

    // Request hw decoding, just for testing.
    mpv::qt::set_option_variant(mpv, "hwdec", "auto");

    mpv_gl = (mpv_opengl_cb_context *)mpv_get_sub_api(mpv, MPV_SUB_API_OPENGL_CB);
    if (!mpv_gl)
        throw std::runtime_error("OpenGL not compiled in");
    mpv_opengl_cb_set_update_callback(mpv_gl, MpvWidget::on_update, (void *)this);
    connect(this, SIGNAL(frameSwapped()), SLOT(swapped()));

    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_set_wakeup_callback(mpv, wakeup, this);

}



MpvWidget::~MpvWidget()
{
    makeCurrent();
    if (mpv_gl)
        mpv_opengl_cb_set_update_callback(mpv_gl, NULL, NULL);
    // Until this call is done, we need to make sure the player remains
    // alive. This is done implicitly with the mpv::qt::Handle instance
    // in this class.
    mpv_opengl_cb_uninit_gl(mpv_gl);
}

void MpvWidget::command(const QVariant& params)
{
    mpv::qt::command_variant(mpv, params);
}

void MpvWidget::setProperty(const QString& name, const QVariant& value)
{
    mpv::qt::set_property_variant(mpv, name, value);
}

QVariant MpvWidget::getProperty(const QString &name) const
{
    return mpv::qt::get_property_variant(mpv, name);
}


void MpvWidget::initializeGL()
{
    int r = mpv_opengl_cb_init_gl(mpv_gl, NULL, get_proc_address, NULL);
    if (r < 0)
        throw std::runtime_error("could not initialize OpenGL");
}

void MpvWidget::paintGL()
{
    mpv_opengl_cb_draw(mpv_gl, defaultFramebufferObject(), width(), -height());
}

void MpvWidget::swapped()
{
    mpv_opengl_cb_report_flip(mpv_gl, 0);
}

void MpvWidget::on_mpv_events()
{
    // Process all events, until the event queue is empty.
    while (mpv) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        handle_mpv_event(event);
    }
}

void MpvWidget::handle_mpv_event(mpv_event *event)
{
    switch (event->event_id) {
    case MPV_EVENT_PROPERTY_CHANGE: {
        mpv_event_property *prop = (mpv_event_property *)event->data;
        if (strcmp(prop->name, "time-pos") == 0) {
            if (prop->format == MPV_FORMAT_DOUBLE) {
                double time = *(double *)prop->data;
                Q_EMIT positionChanged(time);
            }
        } else if (strcmp(prop->name, "duration") == 0) {
            if (prop->format == MPV_FORMAT_DOUBLE) {
                double time = *(double *)prop->data;
                Q_EMIT durationChanged(time);
            }
        }
        break;
    }
    default: ;
        // Ignore uninteresting or unknown events.
    }
}

// Make Qt invoke mpv_opengl_cb_draw() to draw a new/updated video frame.
void MpvWidget::maybeUpdate()
{
    // If the Qt window is not visible, Qt's update() will just skip rendering.
    // This confuses mpv's opengl-cb API, and may lead to small occasional
    // freezes due to video rendering timing out.
    // Handle this by manually redrawing.
    // Note: Qt doesn't seem to provide a way to query whether update() will
    //       be skipped, and the following code still fails when e.g. switching
    //       to a different workspace with a reparenting window manager.
    if (window()->isMinimized()) {
        makeCurrent();
        paintGL();
        context()->swapBuffers(context()->surface());
        swapped();
        doneCurrent();
    } else {
        update();
    }
}

void MpvWidget::on_update(void *ctx)
{
    QMetaObject::invokeMethod((MpvWidget*)ctx, "maybeUpdate");
}

/*************************DanmakuPlayer BEGIN*************************/


DanmakuPlayer::DanmakuPlayer(QWidget *parent, Qt::WindowFlags f) : MpvWidget(parent, f)
{
    setFocusPolicy(Qt::StrongFocus);
    initDanmaku();
}

DanmakuPlayer::~DanmakuPlayer()
{

}

void DanmakuPlayer::initDanmaku()
{
    for(int i = 0; i < 24; i++)
    {
        danmakuChannelSequence[i] = i;
    }

    setRandomSequence(23, 12);
    setRandomSequence(11, 12);

}

bool DanmakuPlayer::isDanmakuVisible()
{
    return danmakuShowFlag;
}

void DanmakuPlayer::launchDanmaku(QString danmakuText)
{
    danmakuFrequency[2]++;
    int danmakuPos = getAvailDanmakuChannel() * (this->height() / 24);
    int danmakuSpeed = this->width() * 10;

    QLabel* danmaku;
    danmaku = new QLabel(this);

    danmaku->setText(danmakuText);

    danmaku->setStyleSheet("color: #FFFFFF; font-size: 18px; font-weight: bold");

    QGraphicsDropShadowEffect *danmakuTextShadowEffect = new QGraphicsDropShadowEffect(this);
    danmakuTextShadowEffect->setColor(QColor("#000000"));
    danmakuTextShadowEffect->setBlurRadius(4);
    danmakuTextShadowEffect->setOffset(1,1);
    danmaku->setGraphicsEffect(danmakuTextShadowEffect);

    QPropertyAnimation* mAnimation=new QPropertyAnimation(danmaku, "pos");
    mAnimation->setStartValue(QPoint(this->width(), danmakuPos));
    mAnimation->setEndValue(QPoint(-500, danmakuPos));
    mAnimation->setDuration(danmakuSpeed);
    mAnimation->setEasingCurve(QEasingCurve::Linear);
    danmaku->show();
    mAnimation->start();

    connect(this, &DanmakuPlayer::closeDanmaku, danmaku, &QLabel::close);
    connect(mAnimation, &QPropertyAnimation::finished, danmaku, &QLabel::deleteLater);
}

int DanmakuPlayer::getAvailDanmakuChannel()
{
    int channel = danmakuChannelSequence[danmakuChannelIndex];
    if(danmakuFrequency[3] >= 4)
    {
        danmakuHighFreqMode = true;
    }
    else
    {
        danmakuHighFreqMode = false;
    }

    if(danmakuHighFreqMode == false)
    {
        if(danmakuFrequency[3] == 0)
        {
//            setRandomSequence(23, 12);
//            setRandomSequence(11, 12);
        }
        danmakuChannelIndex++;
        danmakuChannelIndex = danmakuChannelIndex % 12;
    }
    else
    {
        danmakuChannelIndex++;
        danmakuChannelIndex = danmakuChannelIndex % 24;
    }
    return channel;
}

void DanmakuPlayer::setRandomSequence(int baseIndex, int lengh)
{
    int i;
    int temp = 0;
    if((baseIndex - lengh) < -1)
    {
        lengh = 1 + baseIndex;
    }
    for(i = 0; i < lengh; i++)
    {
        temp = danmakuChannelSequence[baseIndex-i];
        int rdOffset = qrand() % lengh;
        danmakuChannelSequence[baseIndex-i] = danmakuChannelSequence[baseIndex - rdOffset];
        danmakuChannelSequence[baseIndex - rdOffset] = temp;
    }
}

void DanmakuPlayer::updateDanmakuFrequency()
{
    int temp;
    danmakuFrequency[3] = (danmakuFrequency[2] + danmakuFrequency[1] + danmakuFrequency[0]) / 3;
//    danmakuFrequency[3] = danmakuFrequency[2];
    temp = danmakuFrequency[2];
    danmakuFrequency[2] = 0;
    danmakuFrequency[0] = danmakuFrequency[1];
    danmakuFrequency[1] = temp;

}

void DanmakuPlayer::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_D:
        danmakuShowFlag = !danmakuShowFlag;
        if(danmakuShowFlag == false) {
            Q_EMIT closeDanmaku();
        }else {
            initDanmaku();
        }
        break;
    case Qt::Key_F:
        if(!QApplication::activeWindow()->isFullScreen()) {
            QApplication::activeWindow()->showFullScreen();
        }else {
            QApplication::activeWindow()->showNormal();
        }
        break;
    case Qt::Key_Q:
        exit(0);
        break;
    case Qt::Key_Space:
    {
        const bool paused = getProperty("pause").toBool();
        setProperty("pause", !paused);
        break;
    }
    case Qt::Key_M:
    {
        const bool muted = getProperty("ao-mute").toBool();
        setProperty("ao-mute", !muted);
        break;
    }
    case Qt::Key_Minus:
    {
        int volume = getProperty("ao-volume").toInt();
        if(volume > 0)
            volume -= 5;
        setProperty("ao-volume", QString::number(volume));
        break;
    }
    case Qt::Key_Equal:
    {
        int volume = getProperty("ao-volume").toInt();
        if(volume < 100)
            volume += 5;
        setProperty("ao-volume", QString::number(volume));
        break;
    }
    default:
        break;
    }
    MpvWidget::keyPressEvent(event);
}

