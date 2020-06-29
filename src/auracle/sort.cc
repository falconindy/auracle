// SPDX-License-Identifier: MIT
#include "sort.hh"

namespace sort {

template <typename T>
Sorter MakePackageSorter(T aur::Package::*field, OrderBy order_by) {
  switch (order_by) {
    case OrderBy::ORDER_ASC:
      return [=](const aur::Package& a, const aur::Package& b) {
        return (a.*field < b.*field) && !(a.*field > b.*field);
      };
    case OrderBy::ORDER_DESC:
      return [=](const aur::Package& a, const aur::Package& b) {
        return (a.*field > b.*field) && !(a.*field < b.*field);
      };
  }

  return nullptr;
}

Sorter MakePackageSorter(std::string_view field, OrderBy order_by) {
  if (field == "name") {
    return MakePackageSorter(&aur::Package::name, order_by);
  } else if (field == "popularity") {
    return MakePackageSorter(&aur::Package::popularity, order_by);
  } else if (field == "votes") {
    return MakePackageSorter(&aur::Package::votes, order_by);
  } else if (field == "firstsubmitted") {
    return MakePackageSorter(&aur::Package::submitted, order_by);
  } else if (field == "lastmodified") {
    return MakePackageSorter(&aur::Package::modified, order_by);
  }

  return nullptr;
}

}  // namespace sort
