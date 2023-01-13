#ifndef _UTILS_H
#define _UTILS_H

#include <ostream>
#include <vector>
#include <unordered_map>


template <typename S, typename N>
std::ostream &operator<<(std::ostream &os,
                         const std::pair<S, N> &pair)
{
  os << pair.first << ": " << pair.second;
  return os;
}
template <typename S>
std::ostream &operator<<(std::ostream &os,
                         const std::vector<S> &vector)
{
  // Printing all the elements
  // using <<
  os << "[";
  for (S element : vector)
  {
    os << element << ", ";
  }
  os << "]";
  return os;
}

template <typename S, typename T>
std::ostream &operator<<(std::ostream &os,
                         const std::unordered_map<S, T> &vector)
{
  // Printing all the elements
  // using <<
  os << "[";
  for (auto &&element : vector)
  {
    os << element << ", ";
  }
  os << "]";
  return os;
}

template <typename S, size_t N>
std::ostream &operator<<(std::ostream &os,
                         const std::array<S, N> &vector)
{
  // Printing all the elements
  // using <<
  os << "[";
  for (S element : vector)
  {
    os << element << ", ";
  }
  os << "]";
  return os;
}


#endif