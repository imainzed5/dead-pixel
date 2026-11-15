#include "core/Tuning.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

std::unordered_map<std::string, float>& Tuning::values()
{
    static std::unordered_map<std::string, float> store;
    return store;
}

bool Tuning::load(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return true; // no file: every value keeps its default
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const nlohmann::json root = nlohmann::json::parse(buffer.str(), nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
        std::cerr << "Tuning: " << path << " is not a JSON object; using defaults\n";
        return false;
    }
    for (const auto& [key, value] : root.items())
    {
        if (value.is_number())
        {
            values()[key] = value.get<float>();
        }
    }
    std::cout << "Tuning: " << values().size() << " values from " << path << '\n';
    return true;
}

float Tuning::get(std::string_view key, float fallback)
{
    const auto& store = values();
    if (store.empty())
    {
        return fallback;
    }
    const auto it = store.find(std::string(key));
    return it != store.end() ? it->second : fallback;
}

void Tuning::set(std::string_view key, float value)
{
    values()[std::string(key)] = value;
}

void Tuning::clear()
{
    values().clear();
}
