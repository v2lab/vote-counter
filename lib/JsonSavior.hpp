#ifndef JSONSAVIOR_HPP
#define JSONSAVIOR_HPP

class QTextStream;

class JsonSavior
{
public:
    JsonSavior( QTextStream & text_stream )
        : m_textStream(text_stream)
    {}

    void openBrace() { m_textStream << "{\n"; }
    void closeBrace() { m_textStream << "}\n"; }
    void comma() { m_textStream << ",\n"; }

    template <typename T>
    void save(const QString& tag, T data)
    {
        m_textStream << tag << ":" << data;
    }

protected:
    QTextStream & m_textStream;
};

template<typename T>
QTextStream& operator<< (QTextStream& os, const QMap<QString, T>& map)
{
    os << "{\n";
    int count = map.size();
    foreach( QString key, map.keys() ) {
        os << key << ":" << map[key] << ((--count) ? ",\n" : "\n");
    }
    os << "}\n";
    return os;
}

template<typename T>
QTextStream& operator<< (QTextStream& os, const QHash<QString, T>& hash)
{
    os << "{\n";
    int count = hash.size();
    foreach( QString key, hash.keys() ) {
        os << key << ":" << hash[key] << ((--count) ? ",\n" : "\n");
    }
    os << "}\n";
    return os;
}

template<typename T>
QTextStream& operator<< (QTextStream& os, const QVector<T>& vector)
{
    os << "[\n";
    int count = vector.size();
    foreach( T value, vector ) {
        os << value << ((--count) ? ",\n" : "\n");
    }
    os << "]\n";
    return os;
}

template<typename T>
QTextStream& operator<< (QTextStream& os, const QList<T>& list)
{
    os << "[\n";
    int count = list.size();
    foreach( T value, list ) {
        os << value << ((--count) ? ",\n" : "\n");
    }
    os << "]\n";
    return os;
}

QTextStream& operator<< (QTextStream& os, const QPoint& point)
{
    os << (QList<int>() << point.x() << point.y());
    return os;
}

#endif // JSONSAVIOR_HPP
