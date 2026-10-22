#ifndef RECEIVER_H
#define RECEIVER_H

#include "common.h"

#include "MemorySys.h"
#include <slist>

class RefCounted;

/* Receives objects, taking over the reference passed with each. */
class Receiver {
public:
    virtual ~Receiver();
    virtual void Receive(RefCounted* object) = 0;
};

/* An object shared by whoever holds a reference to it; deleted with the last one. */
class RefCounted {
public:
    int m_refCount;

    virtual ~RefCounted();
    virtual void SetRefCount(int count);

    void AddRef() { m_refCount++; }
    void Release()
    {
        if (m_refCount == 1) {
            delete this;
        } else {
            m_refCount--;
        }
    }
};

/* A receiver that can be shared, through a reference count. */
class SharedReceiver : public Receiver, public RefCounted {
public:
    ~SharedReceiver();
};

/* Passes what it is given on to each of its receivers. */
class Broadcaster : public Receiver, public slist<Receiver*> {
public:
    ~Broadcaster();
    void Receive(RefCounted* object);
};

/* Lets go of what it is given. */
class NullReceiver : public Receiver {
public:
    ~NullReceiver();
    void Receive(RefCounted* object);
};

#endif
