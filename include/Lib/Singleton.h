#pragma once

template <class T>
class Singleton {
    static inline T* singleton = nullptr;

    public:
    static T* GetSingleton() { 
        if (singleton == nullptr) {
            singleton = new T();
        }
        return singleton;
    }
};
