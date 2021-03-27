#pragma once
#include <stdexcept>
namespace Kuai
{

    template <typename T>
    struct Option
    {
        char data[sizeof(T)];
        bool hasValue;

        Option(T &&v) : hasValue(true)
        {
            new (data) T(std::forward<T>(v));
        }

        Option() : hasValue(false) {}

        T &get()
        {
            if (!hasValue)
            {
                throw std::runtime_error("Empty option");
            }
            return *(T *)data;
        }

        bool hasData() const
        {
            return hasValue;
        }

        ~Option()
        {
            if (hasValue)
            {
                ((T *)data)->~T();
            }
        }
    };

    /*template <typename T>
    struct Option<T*>
    {
        T* data;

        Option(T* v) :data(v){
        }

        Option():data(nullptr) {}

        T* get() {
            if(!data) {
                throw std::runtime_error("Empty option")
            }
            return data;
        }

        bool hasData() const {
            return data;
        }
    };*/
} // namespace Kuai