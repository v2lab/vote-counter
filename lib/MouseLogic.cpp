#include "MouseLogic.hpp"

#include <QObject>
#include <QGraphicsScene>

MouseLogic::MouseLogic(QGraphicsScene *parent) :
    QObject(parent),
    m_state(IDLE)
{
    parent->installEventFilter(this);
}

bool MouseLogic::eventFilter(QObject *, QEvent *event)
{
    QGraphicsSceneMouseEvent * gsm_event = dynamic_cast<QGraphicsSceneMouseEvent *>(event);

    switch( event->type() ) {
    case QEvent::GraphicsSceneMousePress:
        Q_ASSERT(gsm_event);
        m_state = PRESSED;
        m_button = gsm_event->button();
        m_firstPoint = gsm_event->scenePos();
        return true; // event's handled
    case QEvent::GraphicsSceneMouseRelease:
        Q_ASSERT(gsm_event);
        switch(m_state) {
        case PRESSED:
            emit pointClicked( gsm_event->scenePos(), gsm_event->button(), gsm_event->modifiers() );
            m_state = IDLE;
            return true;
        case SELECTING_RECT:
            if (gsm_event->button() == m_button) {
                m_state = IDLE;
                emit rectSelected( makeRect( m_firstPoint, gsm_event->scenePos() ), gsm_event->button(), gsm_event->modifiers());
            }
            return true;
        }
        break;
    case QEvent::GraphicsSceneMouseMove:
        Q_ASSERT(gsm_event);
        switch(m_state) {
        case PRESSED:
            m_state = SELECTING_RECT;
        case SELECTING_RECT:
            emit rectUpdated( makeRect( m_firstPoint, gsm_event->scenePos() ), m_button, gsm_event->modifiers());
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

QRectF MouseLogic::makeRect(const QPointF &p1, const QPointF &p2)
{
    QPointF tl( std::min(p1.x(), p2.x()), std::min(p1.y(),p2.y())),
            br( std::max(p1.x(), p2.x()), std::max(p1.y(),p2.y()));
    return QRectF( tl, br );
}
