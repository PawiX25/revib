#ifndef OBJECTSWAP_H
#define OBJECTSWAP_H

#include "common.h"

#include "TmdLineObj.h"

/* Shows one of a model's objects in place of another. */
struct ObjectSwap {
    TmdModel* model;
    const int* objects; /* objects[0]: the one replaced; objects[n]: shown for n (0: itself) */
    TmdLineObj* original;

    ObjectSwap(TmdModel* model, const int* objects)
        : model(model), objects(objects), original(model->m_objects[objects[0]])
    {
    }
    void Show(int n)
    {
        if (model != NULL) {
            TmdLineObj* object = n == 0 ? original : model->m_objects[objects[n]];
            model->m_objects[objects[0]] = object;
        }
    }
    ~ObjectSwap() { Show(0); }
};

#endif
