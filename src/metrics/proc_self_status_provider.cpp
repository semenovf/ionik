////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2024 Vladislav Trifochkin
//
// This file is part of `ionik-lib`.
//
// Changelog:
//      2024.09.22 Initial version.
////////////////////////////////////////////////////////////////////////////////
#include "parser.hpp"
#include "ionik/metrics/proc_provider.hpp"
#include "ionik/local_file.hpp"
#include <pfs/i18n.hpp>
#include <pfs/numeric_cast.hpp>
#include <cctype>
#include <unordered_set>

namespace ionik {
namespace metrics {

using string_view = proc_self_status_provider::string_view;

static std::unordered_set<string_view> const KEYS_WITH_UNITS {
      "VmPeak"
    , "VmSize"
    , "VmLck"
    , "VmPin"
    , "VmHWM"
    , "VmRSS"
    , "RssAnon"
    , "RssFile"
    , "RssShmem"
    , "VmData"
    , "VmStk"
    , "VmExe"
    , "VmLib"
    , "VmPTE"
    , "VmSwap"
    , "HugetlbPages"
};

proc_self_status_provider::proc_self_status_provider () = default;

bool proc_self_status_provider::parse_record (std::string::const_iterator & pos
    , std::string::const_iterator last, record_view & rec, error * perr)
{
    auto p = pos;

    if (p == last)
        return false;

    skip_ws(p, last);

    if (p == last)
        return false;

    rec.key = string_view{};
    rec.values.clear();

    auto success = advance_key(p, last, rec.key)
        && advance_colon(p, last)
        && advance_ws(p, last);

    if (success) {
        auto key_with_units = (KEYS_WITH_UNITS.find(rec.key) != KEYS_WITH_UNITS.cend());

        if (key_with_units) {
            rec.values.resize(2);
            success = advance_decimal_digits_value(p, last, rec.values[0])
                && advance_ws(p, last)
                && advance_units(p, last, rec.values[1])
                && advance_ws(p, last)
                && advance_nl(p, last);
        } else {
            rec.values.resize(1);
            success = advance_unparsed_value(p, last, rec.values[0])
                && advance_nl(p, last);
        }
    }

    if (!success) {
        pfs::throw_or(perr, error {
              pfs::errc::unexpected_data
            , tr::_("unexpected /proc/self/status record format")
        });

        return false;
    }

    if (rec.key.empty()) {
        pfs::throw_or(perr, error {
              pfs::errc::unexpected_data
            , tr::_("/proc/self/status record key is empty")
        });

        return false;
    }

    if (rec.values.empty()) {
        pfs::throw_or(perr, error {
              pfs::errc::unexpected_data
            , tr::_("/proc/self/status record value is empty")
        });

        return false;
    }

    pos = p;
    return true;
}

std::map<string_view, proc_self_status_provider::record_view>
proc_self_status_provider::parse (error * perr)
{
    if (!read_all(perr))
        return std::map<string_view, record_view>{};

    auto pos = _content.cbegin();
    auto last = _content.cend();

    record_view rec;
    std::map<string_view, record_view> result;

    while (parse_record(pos, last, rec))
        result.emplace(rec.key, rec);

    return result;
}

bool proc_self_status_provider::read_all (error * perr)
{
    error err;
    proc_reader reader {PFS__LITERAL_PATH("/proc/self/status"), & err};

    if (err) {
        pfs::throw_or(perr, std::move(err));
        return false;
    }

    _content = reader.content();
    return true;
}

}} // namespace ionik::metrics