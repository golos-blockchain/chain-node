#include <boost/algorithm/string.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/plugins/tags/discussion_query.hpp>
#include <golos/plugins/tags/tags_object.hpp>
#include <golos/plugins/tags/tag_visitor.hpp>

namespace golos { namespace plugins { namespace tags {

    void tags_to_lower(std::set<std::string>& tags) {
        auto src = std::move(tags);
        for (const auto& name: src) {
            auto value = boost::trim_copy(name);
            boost::to_lower(value);
            if (!value.empty()) {
                tags.insert(value);
            }
        }
    }

    void discussion_query::prepare() {
        tags_to_lower(select_tags);
        tags_to_lower(filter_tags);
        tags_to_lower(select_languages);
        tags_to_lower(filter_languages);
    }

    void discussion_query::validate() const {
        GOLOS_CHECK_LIMIT_PARAM(limit, 100);

        GOLOS_CHECK_PARAM(filter_tags, {
            for (auto& itr : filter_tags) {
                GOLOS_CHECK_VALUE(select_tags.find(itr) == select_tags.end(),
                    "Can't filter and select tag '${tag}' at the same time",
                    ("tag", itr));
            }
        });

        GOLOS_CHECK_PARAM(filter_languages, {
            for (auto& itr : filter_languages) {
                GOLOS_CHECK_VALUE(select_languages.find(itr) == select_languages.end(),
                    "Can't filter and select language '${language}' at the same time",
                    ("language", itr));
            }
        });
    }

    bool discussion_query::is_good_tags(
        const discussion& d, std::size_t tags_number, std::size_t tag_max_length
    ) const {
        if (!has_tags_selector() && !has_tags_filter() && !has_language_selector() && !has_language_filter() && !filter_tag_masks.size()) {
            return true;
        }

        auto meta = get_metadata(d.json_metadata, tags_number, tag_max_length);
        if ((has_language_selector() && !select_languages.count(meta.language)) ||
            (has_language_filter() && filter_languages.count(meta.language))
        ) {
            return false;
        }

        bool result = select_tags.empty();
        for (auto& name : meta.tags) {
            if (has_tags_filter() && filter_tags.count(name)) {
                return false;
            } else if (!result && select_tags.count(name)) {
                result = true;
            }
            for (const auto& mask : filter_tag_masks) {
                if (!mask.size()) continue;
                if (mask.front() == '-') {
                    if (boost::algorithm::ends_with(name, mask)) {
                        result = false; break;
                    }
                } else {
                    if (boost::algorithm::starts_with(name, mask)) {
                        result = false; break;
                    }
                }
            }
        }

        return result;
    }

    bool discussion_query::is_good_category(
        const discussion& d
    ) const {
        bool result = !select_categories.size() || select_categories.count(d.category);
        if (!result)
            return false;

        if (select_category_masks.size())
            result = false;
        for (const auto& cat : select_category_masks) {
            if (!cat.size()) continue;
            if (boost::algorithm::starts_with(d.category, cat)) {
                result = true;
                break;
            }
        }

        return result;
    }

} } } // golos::plugins::tags

