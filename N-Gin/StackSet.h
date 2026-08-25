#pragma once

#include <vector>
#include <unordered_set>

template<typename T>
class StackSet
{
private:
    std::vector<T> stack;
    std::unordered_set<T> lookup;

public:
    bool Push(const T& value)
    {
        if (lookup.contains(value))  // faster than a function-call that does the same
            return false;

        stack.push_back(value);
        lookup.insert(value);
        return true;
    }

    T Pop()
    {
        T value = stack.back();
        stack.pop_back();
        lookup.erase(value);
        return value;
    }
	
	T Front() const
    {
        T value = stack.back();
        return value;
    }

    bool Contains(const T& value) const
    {
        return lookup.contains(value);
        //deprecated: return lookup.find(value) != lookup.end();  // contains should only be C++20
    }

    bool Empty() const
    {
        return stack.empty();
    }

    size_t Size() const
    {
        return stack.size();
    }
};