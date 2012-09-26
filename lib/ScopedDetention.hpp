#ifndef SCOPEDDETENTION_HPP
#define SCOPEDDETENTION_HPP

template<typename T>
class ScopedDetention {
public:
    ScopedDetention(QSet<T>& jail, const T& detainee) :
        m_jail(jail),
        m_detainee(detainee)
    {
        m_jail << m_detainee;
    }
    ~ScopedDetention()
    {
        m_jail.remove(m_detainee);
    }
protected:
    QSet<T>& m_jail;
    const T& m_detainee;
};

#endif // SCOPEDDETENTION_HPP
