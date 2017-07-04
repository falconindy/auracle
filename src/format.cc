#include "format.hh"

#include <time.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>

#include "terminal.hh"

namespace format {

namespace {

// Copied from boost/io/ios_state.hpp
class ios_flags_saver {
 public:
  typedef std::ios_base state_type;
  typedef std::ios_base::fmtflags aspect_type;

  explicit ios_flags_saver(state_type& s) : s_save_(s), a_save_(s.flags()) {}
  ios_flags_saver(state_type& s, aspect_type const& a)
      : s_save_(s), a_save_(s.flags(a)) {}
  ~ios_flags_saver() { this->restore(); }

  void restore() { s_save_.flags(a_save_); }

 private:
  state_type& s_save_;
  aspect_type const a_save_;

  ios_flags_saver& operator=(const ios_flags_saver&);
};

std::ostream& float_precision(std::ostream& os) {
  return os << std::fixed << std::setprecision(2);
}

class Field {
 public:
  static constexpr int kIndentSize = 17;
  static constexpr int kDelimSize = 2;

  explicit Field(const std::string_view field) : field_(field) {}

 private:
  static constexpr char kFieldDelim[] = ": ";

  friend std::ostream& operator<<(std::ostream& os, const Field& ls) {
    os << std::left << std::setw(15) << ls.field_;
    os << (ls.field_.empty() ? "  " : kFieldDelim);

    return os;
  }

  std::string_view field_;
};
constexpr char Field::kFieldDelim[];

template <typename T>
std::ostream& FormatFieldValue(std::ostream& os, const std::string_view field,
                               const T& value);

template <typename T>
class FieldValueFormatter {
 public:
  FieldValueFormatter(const std::string_view field, const T& value)
      : field_(field), value_(value) {}

  friend std::ostream& operator<<(std::ostream& os,
                                  const FieldValueFormatter& v) {
    return FormatFieldValue(os, v.field_, v.value_);
  }

 private:
  const std::string_view field_;
  const T& value_;
};

template <typename T>
FieldValueFormatter<T> FieldValue(const std::string_view field, const T& v) {
  return FieldValueFormatter<T>(field, v);
}

template <typename T>
std::ostream& FormatFieldValue(std::ostream& os, const std::string_view field,
                               const T& value) {
  ios_flags_saver ifs(os);

  return os << Field(field) << float_precision << value << std::endl;
}

template <>
std::ostream& FormatFieldValue(std::ostream& os, const std::string_view field,
                               const std::vector<std::string>& value) {
  if (value.empty()) return os;

  const int columns = terminal::Columns();
  int line_size = 0;

  os << Field(field) << value[0];
  for (size_t i = 1; i < value.size(); ++i) {
    const int item_size = 3 + value[i].size();

    if (columns > 0 &&
        (line_size + item_size) >= (columns - Field::kIndentSize)) {
      os << std::endl << Field("");
      line_size = 0;
    } else {
      os << "  ";
    }

    os << value[i];
    line_size += item_size;
  }

  return os << std::endl;
}

template <>
std::ostream& FormatFieldValue(std::ostream& os, const std::string_view field,
                               const std::vector<aur::Dependency>& value) {
  if (value.empty()) return os;

  const int columns = terminal::Columns();
  int line_size = 0;

  os << Field(field) << value[0].depstring;
  for (size_t i = 1; i < value.size(); ++i) {
    const int item_size = 3 + value[i].depstring.size();

    if (columns > 0 &&
        (line_size + item_size) >= (columns - Field::kIndentSize)) {
      os << std::endl << Field("");
      line_size = 0;
    } else {
      os << "  ";
    }

    os << value[i].depstring;
    line_size += item_size;
  }

  return os << std::endl;
}

template <>
std::ostream& FormatFieldValue(std::ostream& os, const std::string_view field,
                               const time_t& value) {
  struct tm t;
  localtime_r(&value, &t);

  return os << Field(field) << std::put_time(&t, "%c") << std::endl;
}

template <>
std::ostream& FormatFieldValue(std::ostream& os, const std::string_view field,
                               const double& value) {
  return os << Field(field) << float_precision << value << std::endl;
}

// Specialization for optdepends since we don't treat them the same as other
struct OptDepends {
  OptDepends(const std::vector<std::string> value) : value(value) {}

  const std::vector<std::string> value;
};

std::ostream& FormatFieldValue(std::ostream& os, const std::string_view field,
                               const OptDepends& o) {
  if (o.value.empty()) {
    return os;
  }

  os << FieldValue("Optional Deps", o.value[0]);
  for (size_t i = 1; i < o.value.size(); ++i) {
    os << FieldValue("", o.value[i]);
  }

  return os;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const NameOnly& n) {
  return os << terminal::Bold(n.package.name) << std::endl;
}

std::ostream& operator<<(std::ostream& os, const Short& s) {
  namespace t = terminal;

  const auto& p = s.package;
  const auto ood_color = p.out_of_date ? &t::BoldRed : &t::BoldGreen;

  ios_flags_saver ifs(os);

  os << t::BoldMagenta("aur/") << t::Bold(p.name) << " " << ood_color(p.version)
     << " (" << p.votes << ", " << float_precision << p.popularity << ")"
     << std::endl
     << "    " << p.description << std::endl;

  return os;
}

std::ostream& operator<<(std::ostream& os, const Long& l) {
  namespace t = terminal;

  const auto& p = l.package;
  const auto ood_color = p.out_of_date ? &t::BoldRed : &t::BoldGreen;

  os << FieldValue("Repository", t::BoldMagenta("aur"));
  os << FieldValue("Name", p.name);
  os << FieldValue("Version", ood_color(p.version));

  if (p.name != p.pkgbase) {
    os << FieldValue("PackageBase", p.pkgbase);
  }

  os << FieldValue("URL", t::BoldCyan(p.upstream_url));
  os << FieldValue("AUR Page",
                   t::BoldCyan("https://aur.archlinux.org/packages/" + p.name));
  os << FieldValue("Keywords", p.keywords);
  os << FieldValue("Groups", p.groups);
  os << FieldValue("Depends On", p.depends);
  os << FieldValue("Makedepends", p.makedepends);
  os << FieldValue("Checkdepends", p.checkdepends);
  os << FieldValue("Provides", p.provides);
  os << FieldValue("Conflicts With", p.conflicts);
  os << FieldValue("Optional Deps", OptDepends(p.optdepends));
  os << FieldValue("Replaces", p.replaces);
  os << FieldValue("Licenses", p.licenses);
  os << FieldValue("Votes", p.votes);
  os << FieldValue("Popularity", p.popularity);
  os << FieldValue("Maintainer",
                   p.maintainer.empty() ? "(orphan)" : p.maintainer);
  os << FieldValue("Submitted", p.submitted_s);
  os << FieldValue("Last Modified", p.modified_s);
  if (p.out_of_date) {
    os << FieldValue("Out of Date", p.out_of_date);
  }
  os << FieldValue("Description", p.description);

  return os;
}

std::ostream& operator<<(std::ostream& os, const Update& u) {
  namespace t = terminal;

  os << t::Bold(u.from.pkgname) << " " << t::BoldRed(u.from.pkgver) << " -> "
     << t::BoldGreen(u.to.version) << std::endl;

  return os;
}

}  // namespace format
