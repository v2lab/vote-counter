#ifndef MOUSELOGIC_HPP
#define MOUSELOGIC_HPP

#include <QtCore>
#include <QtGui>

class MouseLogic : public QObject
{
    Q_OBJECT
public:
    enum State {
        IDLE,
        PRESSED,
        SELECTING_RECT
    };

    explicit MouseLogic(QGraphicsScene *parent);
    State state() const { return m_state; }

signals:
    void pointClicked(QPointF point, Qt::MouseButton button, Qt::KeyboardModifiers mods);
    void rectUpdated(QRectF rect, Qt::MouseButton button, Qt::KeyboardModifiers mods);
    void rectSelected(QRectF rect, Qt::MouseButton button, Qt::KeyboardModifiers mods);

public slots:

protected:
    virtual bool eventFilter(QObject * target, QEvent * event);
    static QRectF makeRect(const QPointF& p1, const QPointF& p2);

    State m_state;
    QPointF m_firstPoint;
    Qt::MouseButton m_button;
};

#endif // MOUSELOGIC_HPP
