#pragma once
#include <string_view>
#include <string>

extern int ai_family_order_table[7][3];
extern int DNS_order;

std::string FQDN(std::string_view host);
