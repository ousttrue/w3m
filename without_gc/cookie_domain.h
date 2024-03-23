#pragma once
#include <string_view>
#include <string>
#include <optional>
#include <list>

extern std::string cookie_reject_domains;
extern std::string cookie_accept_domains;
extern std::string cookie_avoid_wrong_number_of_dots;
extern std::list<std::string> Cookie_reject_domains;
extern std::list<std::string> Cookie_accept_domains;
extern std::list<std::string> Cookie_avoid_wrong_number_of_dots_domains;
extern int ai_family_order_table[7][3];
extern int DNS_order;

std::string FQDN(std::string_view host);

/// if match, return host.substr
std::optional<std::string_view> domain_match(std::string_view host,
                                             std::string_view _domain);
