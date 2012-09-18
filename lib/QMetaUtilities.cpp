#include "QMetaUtilities.hpp"

void QMetaUtilities::connectSlotsByName(QObject * source, QObject * target)
{
    if (!source || !target) return;

    const QMetaObject * source_mo = source->metaObject();
    const QMetaObject * target_mo = target->metaObject();
    Q_ASSERT(source_mo);
    Q_ASSERT(target_mo);

    // find source's children
    // and add source itself to the list, so we can autoconnect its signals too
    const QObjectList list = source->findChildren<QObject *>(QString()) << source;


    for (int i = 0; i < target_mo->methodCount(); ++i) {
        const char *slot = target_mo->method(i).signature();
        Q_ASSERT(slot);
        if (slot[0] != 'o' || slot[1] != 'n' || slot[2] != '_')
            continue;
        bool foundIt = false;
        for(int j = 0; j < list.count(); ++j) {
            const QObject *co = list.at(j);
            const QMetaObject *smo = co->metaObject();
            QByteArray objName = co->objectName().toAscii();
            int len = objName.length();
            if (!len || qstrncmp(slot + 3, objName.data(), len) || slot[len+3] != '_')
                continue;
            int sigIndex = smo->indexOfSignal(slot + len + 4);
            if (sigIndex < 0) { // search for compatible signals
                int slotlen = qstrlen(slot + len + 4) - 1;
                for (int k = 0; k < co->metaObject()->methodCount(); ++k) {
                    QMetaMethod method = smo->method(k);
                    if (method.methodType() != QMetaMethod::Signal)
                        continue;

                    if (!qstrncmp(method.signature(), slot + len + 4, slotlen)) {
                        sigIndex = k;
                        break;
                    }
                }
            }
            if (sigIndex < 0)
                continue;
#warning Does this work?
            if (QMetaObject::connect(co, sigIndex, target, i)) {
                foundIt = true;
                break;
            }
        }
        if (foundIt) {
            // we found our slot, now skip all overloads
            while (target_mo->method(i + 1).attributes() & QMetaMethod::Cloned)
                  ++i;
        } else if (!(target_mo->method(i).attributes() & QMetaMethod::Cloned)) {
            qWarning("QMetaObject::connectSlotsByName: No matching signal for %s", slot);
        }
    }
}
