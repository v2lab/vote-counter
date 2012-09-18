#ifndef QMETAUTILITIES_HPP
#define QMETAUTILITIES_HPP

class QMetaUtilities
{
public:
    static void connectSlotsByName(QObject * source, QObject * target);
};

#endif // QMETAUTILITIES_HPP
