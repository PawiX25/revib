#include "common.h"

#include <stdlib.h>

#include "Format.h"
#include "Receiver.h"

/* the allocator's "out of memory" string, of a shared header */
INCLUDE_RODATA("asm/main_g/nonmatchings/Receiver", D_80019BAC);

Receiver::~Receiver()
{
}

RefCounted::~RefCounted()
{
}

void RefCounted::SetRefCount(int count)
{
    m_refCount = count;
}

SharedReceiver::~SharedReceiver()
{
}

Broadcaster::~Broadcaster()
{
}

void Broadcaster::Receive(RefCounted* object)
{
    for (const_iterator i = begin(); i != end(); ++i) {
        object->AddRef();
        (*i)->Receive(object);
    }
    object->Release();
}

NullReceiver::~NullReceiver()
{
}

void NullReceiver::Receive(RefCounted* object)
{
    object->Release();
}
