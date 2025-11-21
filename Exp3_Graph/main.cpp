#include "pathfinder.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <algorithm>

using namespace chr;

// 文件/程序说明：
// 这是一个简单的命令行地图导航工具的入口与交互层。
// 负责解析命令行与交互输入，将操作转发到 plat（地图平台）对象上。
// 注：此文件只包含交互逻辑，核心数据结构与算法在 pathfinder.* 中实现。

void print_help() {
    std::cout << "========== 地图导航系统命令行模式 ==========\n";
    std::cout << "命令格式: -command [参数]\n";
    std::cout << "可用命令:\n";
    std::cout << "  -load <path>                   加载地图\n";
    std::cout << "  -save <path>                   保存地图\n";
    std::cout << "  -city -add <id> <name>         添加城市\n";
    std::cout << "  -city -del <id_or_name>        删除城市\n";
    std::cout << "  -city -rename <id_or_name> <new_name>  重命名城市\n";
    std::cout << "  -loc -add <city_id_or_name> <serial> <name> <lon> <lat>  添加地点\n";
    std::cout << "  -loc -del <city_id_or_name> <serial_or_name>  删除地点\n";
    std::cout << "  -loc -rename <city_id_or_name> <serial_or_name> <new_name>  重命名地点\n";
    std::cout << "  -road -add -uni <from_id_or_name> <to_id_or_name>    添加单向路\n";
    std::cout << "  -road -add -bi <from_id_or_name> <to_id_or_name>     添加双向路\n";
    std::cout << "  -road -del -uni <from_id_or_name> <to_id_or_name>    删除单向路\n";
    std::cout << "  -road -del -bi <from_id_or_name> <to_id_or_name>     删除双向路\n";
    std::cout << "  -search -locs <keyword>        查询地点\n";
    std::cout << "  -search -path <from_id_or_name> <to_id_or_name> 路径查询\n";
    std::cout << "  -show -cities                   显示所有城市\n";
    std::cout << "  -show -locs <city_id_or_name> 显示城市的所有地点\n";
    std::cout << "  -show -locs -all              显示所有地点（按城市分块）\n";
    std::cout << "  -show -roads -of <city_id_or_name> 显示城市所有道路（包括向其他城市的）\n";
    std::cout << "  -show -roads -from <from_id_or_name> 显示源于地点的所有道路\n";
    std::cout << "  -show -roads -to <to_id_or_name>    显示到达地点的所有道路\n";
    std::cout << "  -show -roads -all             显示所有道路（按城市分块）\n";
    std::cout << "  -clear                         清空屏幕\n";
    std::cout << "  -batch <file_path>             批量执行命令文件\n";
    std::cout << "  -exit                          退出\n";
    std::cout << "  -help                          显示帮助\n";
}

// 解析城市 id 或名称：
// - 若输入为纯数字则直接转换为 id
// - 否则使用 plat::fuzzy_find_towns 进行模糊匹配并在多选时提示用户选择
unsigned parse_town_id_or_name(plat& p, const std::string& input) {
    try {
        return std::stoul(input);
    }
    catch (const std::exception&) {
        auto results = p.fuzzy_find_towns(input);
        if (results.empty()) {
            throw std::runtime_error("未找到匹配的城市: " + input);
        }
        else if (results.size() == 1) {
            return results[0].first;
        }
        else {
            std::cout << "针对'" << input << "'找到多个匹配的城市，请选择:\n";
            for (size_t i = 0; i < results.size(); i++) {
                std::cout << i + 1 << ": " << results[i].second << " (ID: " << results[i].first << ")\n";
            }
            std::cout << "请输入编号: ";
            int choice;
            std::cin >> choice;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (choice < 1 || choice > results.size()) {
                throw std::runtime_error("无效的选择");
            }
            return results[choice - 1].first;
        }
    }
}

// 解析地点 id 或名称（全局 place id）
// 同样支持数字输入或模糊匹配并交互选择
size_t parse_place_id_or_name(plat& p, const std::string& input) {
    try {
        return std::stoull(input);
    }
    catch (const std::exception&) {
        auto results = p.fuzzy_find_places(input);
        if (results.empty()) {
            throw std::runtime_error("未找到匹配的地点: " + input);
        }
        else if (results.size() == 1) {
            return results[0].first;
        }
        else {
            std::cout << "针对'" << input << "'找到多个匹配的地点，请选择:\n";
            for (size_t i = 0; i < results.size(); i++) {
                std::cout << i + 1 << ": " << results[i].second << " (ID: " << results[i].first << ")\n";
            }
            std::cout << "请输入编号: ";
            int choice;
            std::cin >> choice;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (choice < 1 || choice > results.size()) {
                throw std::runtime_error("无效的选择");
            }
            return results[choice - 1].first;
        }
    }
}

// 解析城市内地点序列号或名称（局部序号）
// 注意：低 32 位为序列号，因此返回 unsigned serial
unsigned parse_local_place_serial_or_name(plat& p, unsigned town_id, const std::string& input) {
    try {
        return std::stoul(input);
    }
    catch (const std::exception&) {
        auto town_ptr = p.town(town_id);
        if (!town_ptr) {
            throw std::runtime_error("城市不存在");
        }

        std::vector<std::pair<unsigned, std::string>> results;
        std::string lower_input = input;
        std::transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);

        for (const auto& [place_id, place_ptr] : town_ptr->places()) {
            unsigned serial = place_id & 0xFFFFFFFF;
            std::string lower_name = place_ptr->name();
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);

            if (lower_name.find(lower_input) != std::string::npos) {
                results.emplace_back(serial, place_ptr->name());
            }
        }

        if (results.empty()) {
            throw std::runtime_error("在城市中未找到匹配的地点: " + input);
        }
        else if (results.size() == 1) {
            return results[0].first;
        }
        else {
            std::cout << "针对'" << input << "'找到多个匹配的地点，请选择:\n";
            for (size_t i = 0; i < results.size(); i++) {
                std::cout << i + 1 << ": " << results[i].second << " (序列号: " << results[i].first << ")\n";
            }
            std::cout << "请输入编号: ";
            int choice;
            std::cin >> choice;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (choice < 1 || choice > results.size()) {
                throw std::runtime_error("无效的选择");
            }
            return results[choice - 1].first;
        }
    }
}

// 以下为若干显示帮助函数：列出城市、地点、道路等信息（用于 -show 子命令）
void show_cities(plat& p) {
    std::cout << "\n--- 所有城市 ---\n";
    auto city_ids = p.get_all_town_ids();

    if (city_ids.empty()) {
        std::cout << "暂无城市数据\n";
        return;
    }

    std::cout << "城市总数: " << city_ids.size() << "\n\n";
    for (unsigned city_id : city_ids) {
        auto town_ptr = p.town(city_id);
        std::cout << "城市" << city_id << ": " << town_ptr->name()
            << " (包含 " << town_ptr->places().size() << " 个地点)\n";
    }
}

void show_locations_of_city(plat& p, unsigned city_id) {
    auto town_ptr = p.town(city_id);
    if (!town_ptr) {
        std::cout << "错误: 城市不存在\n";
        return;
    }

    std::cout << "\n--- 城市" << city_id << " (" << town_ptr->name() << ") 的所有地点 ---\n";
    auto& places = town_ptr->places();

    if (places.empty()) {
        std::cout << "该城市暂无地点数据\n";
        return;
    }

    std::cout << "地点总数: " << places.size() << "\n\n";
    for (const auto& [place_id, place_ptr] : places) {
        unsigned serial = place_id & 0xFFFFFFFF;
        std::cout << "地点" << serial << ": " << place_ptr->name()
            << " (经度: " << place_ptr->longitude()
            << ", 纬度: " << place_ptr->latitude() << ")\n";
    }
}

void show_all_locations_grouped(plat& p) {
    std::cout << "\n--- 所有地点（按城市分组） ---\n";
    auto city_ids = p.get_all_town_ids();

    if (city_ids.empty()) {
        std::cout << "暂无城市数据\n";
        return;
    }

    int total_locations = 0;
    for (unsigned city_id : city_ids) {
        auto town_ptr = p.town(city_id);
        auto& places = town_ptr->places();

        std::cout << "\n城市" << city_id << ": " << town_ptr->name()
            << " (" << places.size() << " 个地点)\n";

        for (const auto& [place_id, place_ptr] : places) {
            unsigned serial = place_id & 0xFFFFFFFF;
            std::cout << "  - 地点" << serial << ": " << place_ptr->name()
                << " (经度: " << place_ptr->longitude()
                << ", 纬度: " << place_ptr->latitude() << ")\n";
            total_locations++;
        }
    }
    std::cout << "\n总计: " << total_locations << " 个地点\n";
}

// 显示城市内所有道路（包含跨城市的出边），并标注单/双向与跨城市标识
void show_roads_of_city(plat& p, unsigned city_id) {
    auto town_ptr = p.town(city_id);
    if (!town_ptr) {
        std::cout << "错误: 城市不存在\n";
        return;
    }

    std::cout << "\n--- 城市" << city_id << " (" << town_ptr->name() << ") 的所有道路 ---\n";
    auto& places = town_ptr->places();

    if (places.empty()) {
        std::cout << "该城市暂无地点数据\n";
        return;
    }

    int road_count = 0;
    for (const auto& [from_id, from_place] : places) {
        for (const auto& [to_id, distance] : from_place->roads()) {
            auto to_place = p.place(to_id);
            if (!to_place) continue;

            unsigned to_city_id = to_id >> 32;
            std::string road_type = (to_place->has_road_to(from_id)) ? "双向" : "单向";
            std::string city_info = (to_city_id == city_id) ? "" : " [跨城市]";

            std::cout << road_type << "道路" << ": "
                << from_place->name() << " -> " << to_place->name()
                << " (" << distance_to_string(distance) << ")" << city_info << "\n";
            road_count++;
        }
    }

    if (road_count == 0) {
        std::cout << "该城市暂无道路数据\n";
    }
    else {
        std::cout << "\n道路总数: " << road_count << "\n";
    }
}

void show_roads_from_place(plat& p, size_t from_id) {
    auto from_place = p.place(from_id);
    if (!from_place) {
        std::cout << "错误: 起点地点不存在\n";
        return;
    }

    unsigned city_id = from_id >> 32;
    auto town_ptr = p.town(city_id);

    std::cout << "\n--- 从地点 " << from_place->name() << " 出发的所有道路 ---\n";
    auto& roads = from_place->roads();

    if (roads.empty()) {
        std::cout << "该地点暂无出发道路\n";
        return;
    }

    for (const auto& [to_id, distance] : roads) {
        auto to_place = p.place(to_id);
        if (!to_place) continue;

        unsigned to_city_id = to_id >> 32;
        std::string road_type = (to_place->has_road_to(from_id)) ? "双向" : "单向";
        std::string city_info = (to_city_id == city_id) ? "" : " [跨城市]";

        std::cout << road_type << "道路" << ": "
            << from_place->name() << " -> " << to_place->name()
            << " (" << distance_to_string(distance) << ")" << city_info << "\n";
    }

    std::cout << "\n出发道路总数: " << roads.size() << "\n";
}

void show_roads_to_place(plat& p, size_t to_id) {
    auto to_place = p.place(to_id);
    if (!to_place) {
        std::cout << "错误: 终点地点不存在\n";
        return;
    }

    unsigned city_id = to_id >> 32;
    auto town_ptr = p.town(city_id);

    std::cout << "\n--- 到达地点 " << to_place->name() << " 的所有道路 ---\n";

    int road_count = 0;
    // 搜索所有城市中指向该地点的道路
    auto city_ids = p.get_all_town_ids();
    for (unsigned search_city_id : city_ids) {
        auto search_town = p.town(search_city_id);
        for (const auto& [from_id, from_place] : search_town->places()) {
            if (from_place->has_road_to(to_id)) {
                double distance = from_place->road_length_to(to_id);
                std::string road_type = (to_place->has_road_to(from_id)) ? "双向" : "单向";
                std::string city_info = (search_city_id == city_id) ? "" : " [跨城市]";

                std::cout << road_type << "道路" << ": "
                    << from_place->name() << " -> " << to_place->name()
                    << " (" << distance_to_string(distance) << ")" << city_info << "\n";
                road_count++;
            }
        }
    }

    if (road_count == 0) {
        std::cout << "暂无到达该地点的道路\n";
    }
    else {
        std::cout << "\n到达道路总数: " << road_count << "\n";
    }
}

void show_all_roads_grouped(plat& p) {
    std::cout << "\n--- 所有道路（按城市分组） ---\n";
    auto city_ids = p.get_all_town_ids();

    if (city_ids.empty()) {
        std::cout << "暂无城市数据\n";
        return;
    }

    int total_roads = 0;
    for (unsigned city_id : city_ids) {
        auto town_ptr = p.town(city_id);
        auto& places = town_ptr->places();

        int city_road_count = 0;
        std::cout << "\n城市" << city_id << ": " << town_ptr->name() << "\n";

        for (const auto& [from_id, from_place] : places) {
            for (const auto& [to_id, distance] : from_place->roads()) {
                auto to_place = p.place(to_id);
                if (!to_place) continue;

                unsigned to_city_id = to_id >> 32;
                std::string road_type = (to_place->has_road_to(from_id)) ? "双向" : "单向";
                std::string city_info = (to_city_id == city_id) ? "" : " [跨城市]";

                std::cout << road_type << "道路" << ": "
                    << from_place->name() << " -> " << to_place->name()
                    << " (" << distance_to_string(distance) << ")" << city_info << "\n";
                city_road_count++;
                total_roads++;
            }
        }

        if (city_road_count == 0) {
            std::cout << "  暂无道路数据\n";
        }
        else {
            std::cout << "  道路数量: " << city_road_count << "\n";
        }
    }

    std::cout << "\n道路总计: " << total_roads << "\n";
}

// 解析并执行单条命令（供命令行参数或交互使用）
// 该函数包含大量分支，对每个子命令均有注释说明用途
bool parse_command(int argc, char* argv[], plat& p) {
    if (argc < 2) {
        std::cout << "错误: 缺少命令参数\n";
        print_help();
        return false;
    }

    std::string command = argv[1];

    if (command == "-help") {
        print_help();
        return true;
    }
    else if (command == "-load") {
        if (argc < 3) {
            std::cout << "错误: 缺少文件路径参数\n";
            return false;
        }
        std::string path = argv[2];
        if (path.starts_with("\"") && path.ends_with("\"")) {
            path = path.substr(1, path.length() - 2);
        }
        try {
            p.load_all_cities_from_json(path);
        }
        catch (const std::exception& e) {
            std::cout << "加载失败: " << e.what() << std::endl;
        }
    }
    else if (command == "-save") {
        if (argc < 3) {
            std::cout << "错误: 缺少文件路径参数\n";
            return false;
        }
        std::string path = argv[2];
        if (path.starts_with("\"") && path.ends_with("\"")) {
            path = path.substr(1, path.length() - 2);
        }
        try {
            p.save_all_cities_as_json(path);
            std::cout << "地图保存成功!\n";
        }
        catch (const std::exception& e) {
            std::cout << "保存失败: " << e.what() << std::endl;
        }
    }
    else if (command == "-city") {
        if (argc < 3) {
            std::cout << "错误: 缺少城市操作参数\n";
            return false;
        }
        std::string op = argv[2];
        if (op == "-add") {
            if (argc < 5) {
                std::cout << "错误: 缺少城市ID或名称参数\n";
                return false;
            }
            unsigned id = std::stoul(argv[3]);
            std::string name = argv[4];
            try {
                p.add_town(id, name);
                std::cout << "城市添加成功!\n";
            }
            catch (const std::exception& e) {
                std::cout << "错误: " << e.what() << std::endl;
            }
        }
        else if (op == "-del") {
            if (argc < 4) {
                std::cout << "错误: 缺少城市ID或名称\n";
                return false;
            }
            try {
                unsigned id = parse_town_id_or_name(p, argv[3]);
                if (p.remove_town(id)) {
                    std::cout << "城市删除成功!\n";
                }
                else {
                    std::cout << "城市不存在!\n";
                }
            }
            catch (const std::exception& e) {
                std::cout << "错误: " << e.what() << std::endl;
            }
        }
        else if (op == "-rename") {
            if (argc < 5) {
                std::cout << "错误: 缺少城市ID或名称或新名称参数\n";
                return false;
            }
            try {
                unsigned id = parse_town_id_or_name(p, argv[3]);
                std::string new_name = argv[4];
                if (p.rename_town(id, new_name)) {
                    std::cout << "城市重命名成功!\n";
                }
                else {
                    std::cout << "城市不存在!\n";
                }
            }
            catch (const std::exception& e) {
                std::cout << "错误: " << e.what() << std::endl;
            }
        }
        else {
            std::cout << "错误: 未知的城市操作参数: " << op << "\n";
            return false;
        }
    }
    else if (command == "-loc") {
        if (argc < 3) {
            std::cout << "错误: 缺少地点操作参数\n";
            return false;
        }
        std::string op = argv[2];
        if (op == "-add") {
            if (argc < 8) {
                std::cout << "错误: 缺少地点参数\n";
                return false;
            }
            try {
                unsigned city_id = parse_town_id_or_name(p, argv[3]);
                unsigned serial = std::stoul(argv[4]);
                std::string name = argv[5];
                double lon = std::stod(argv[6]);
                double lat = std::stod(argv[7]);

                auto town_ptr = p.town(city_id);
                if (!town_ptr) {
                    std::cout << "城市不存在!\n";
                    return false;
                }

                try {
                    town_ptr->add_local_place(serial, name, { lat, lon });
                    std::cout << "地点添加成功!\n";
                }
                catch (const std::exception& e) {
                    std::cout << "错误: " << e.what() << std::endl;
                }
            }
            catch (const std::exception& e) {
                std::cout << "错误: " << e.what() << std::endl;
            }
        }
        else if (op == "-del") {
            if (argc < 5) {
                std::cout << "错误: 缺少城市ID或名称和地点序列号或名称\n";
                return false;
            }
            try {
                unsigned city_id = parse_town_id_or_name(p, argv[3]);
                unsigned serial = parse_local_place_serial_or_name(p, city_id, argv[4]);

                auto town_ptr = p.town(city_id);
                if (!town_ptr) {
                    std::cout << "城市不存在!\n";
                    return false;
                }

                if (town_ptr->remove_local_place(serial)) {
                    std::cout << "地点删除成功!\n";
                }
                else {
                    std::cout << "地点不存在!\n";
                }
            }
            catch (const std::exception& e) {
                std::cout << "错误: " << e.what() << std::endl;
            }
        }
        else if (op == "-rename") {
            if (argc < 6) {
                std::cout << "错误: 缺少城市ID或名称、地点序列号或名称和新名称参数\n";
                return false;
            }
            try {
                unsigned city_id = parse_town_id_or_name(p, argv[3]);
                unsigned serial = parse_local_place_serial_or_name(p, city_id, argv[4]);
                std::string new_name = argv[5];

                auto town_ptr = p.town(city_id);
                if (!town_ptr) {
                    std::cout << "城市不存在!\n";
                    return false;
                }

                if (p.rename_place(city::place_id(city_id, serial), new_name)) {
                    std::cout << "地点重命名成功!\n";
                }
                else {
                    std::cout << "地点不存在!\n";
                }
            }
            catch (const std::exception& e) {
                std::cout << "错误: " << e.what() << std::endl;
            }
        }
        else {
            std::cout << "错误: 未知的地点操作参数: " << op << "\n";
            return false;
        }
    }
    else if (command == "-road") {
        if (argc < 4) {
            std::cout << "错误: 缺少道路操作参数\n";
            return false;
        }
        std::string op = argv[2];
        std::string type = argv[3];

        if (argc < 6) {
            std::cout << "错误: 缺少起点和终点ID或名称\n";
            return false;
        }
        try {
            size_t from = parse_place_id_or_name(p, argv[4]);
            size_t to = parse_place_id_or_name(p, argv[5]);

            if (op == "-add") {
                if (type == "-uni") {
                    try {
                        double length = p.add_road(from, to);
                        std::cout << "单向道路添加成功! 长度: " << length << "米\n";
                    }
                    catch (const std::exception& e) {
                        std::cout << "错误: " << e.what() << std::endl;
                    }
                }
                else if (type == "-bi") {
                    try {
                        double length = p.add_bidirectional_road(from, to);
                        std::cout << "双向道路添加成功! 长度: " << length << "米\n";
                    }
                    catch (const std::exception& e) {
                        std::cout << "错误: " << e.what() << std::endl;
                    }
                }
                else {
                    std::cout << "错误: 未知的道路类型: " << type << "\n";
                    return false;
                }
            }
            else if (op == "-del") {
                if (type == "-uni") {
                    try {
                        unsigned city_id = from >> 32;
                        if (p.town(city_id)->place(from)->remove_road(to))
                            std::cout << "单向道路删除成功!\n";
                        else
                            std::cout << "单向道路删除失败!\n";
                    }
                    catch (const std::exception& e) {
                        std::cout << "错误: " << e.what() << std::endl;
                    }
                }
                else if (type == "-bi") {
                    try {
                        unsigned city_id = from >> 32;
                        bool success1 = false, success2 = false;

                        if (p.town(city_id)->place(from)->remove_road(to)) {
                            success1 = true;
                            std::cout << "去程道路删除成功!\n";
                        }
                        else {
                            std::cout << "去程道路删除失败!\n";
                        }

                        city_id = to >> 32;
                        if (p.town(city_id)->place(to)->remove_road(from)) {
                            success2 = true;
                            std::cout << "回程道路删除成功!\n";
                        }
                        else {
                            std::cout << "回程道路删除失败!\n";
                        }

                        if (success1 && success2) {
                            std::cout << "双向道路删除成功!\n";
                        }
                    }
                    catch (const std::exception& e) {
                        std::cout << "错误: " << e.what() << std::endl;
                    }
                }
                else {
                    std::cout << "错误: 未知的道路类型: " << type << "\n";
                    return false;
                }
            }
            else {
                std::cout << "错误: 未知的道路操作参数: " << op << "\n";
                return false;
            }
        }
        catch (const std::exception& e) {
            std::cout << "错误: " << e.what() << std::endl;
        }
    }
    else if (command == "-search") {
        if (argc < 3) {
            std::cout << "错误: 缺少搜索类型参数\n";
            return false;
        }
        std::string type = argv[2];
        if (type == "-locs") {
            if (argc < 4) {
                std::cout << "错误: 缺少搜索关键词\n";
                return false;
            }
            std::string keyword = argv[3];
            auto results = p.fuzzy_find_places(keyword);

            if (results.empty()) {
                std::cout << "未找到匹配的地点\n";
            }
            else {
                std::cout << "找到 " << results.size() << " 个匹配地点:\n";
                for (const auto& [id, name] : results) {
                    unsigned city_id = id >> 32;
                    unsigned serial = id & 0xFFFFFFFF;
                    std::cout << "ID: " << id << " (城市" << city_id << "-地点" << serial << "), 名称: " << name << std::endl;
                }
            }
        }
        else if (type == "-path") {
            if (argc < 5) {
                std::cout << "错误: 缺少起点和终点ID或名称\n";
                return false;
            }
            try {
                size_t from = parse_place_id_or_name(p, argv[3]);
                size_t to = parse_place_id_or_name(p, argv[4]);
                auto path = p.find_path(from, to);

                if (path.empty()) {
                    std::cout << "未找到路径\n";
                }
                else {
                    std::cout << "找到路径:\n";
                    p.print_path(path);
                }
            }
            catch (const std::exception& e) {
                std::cout << "错误: " << e.what() << std::endl;
            }
        }
        else {
            std::cout << "错误: 未知的搜索类型: " << type << "\n";
            return false;
        }
    }
    else if (command == "-show") {
        if (argc < 3) {
            std::cout << "错误: -show 命令需要指定显示类型\n";
            print_help();
            return false;
        }

        std::string show_type = argv[2];

        if (show_type == "-cities") {
            show_cities(p);
        }
        else if (show_type == "-locs") {
            if (argc < 4) {
                std::cout << "错误: -show -locs 需要指定城市ID或名称，或使用 -all 参数\n";
                return false;
            }

            std::string locs_param = argv[3];
            if (locs_param == "-all") {
                show_all_locations_grouped(p);
            }
            else {
                try {
                    unsigned city_id = parse_town_id_or_name(p, locs_param);
                    show_locations_of_city(p, city_id);
                }
                catch (const std::exception& e) {
                    std::cout << "错误: " << e.what() << std::endl;
                    return false;
                }
            }
        }
        else if (show_type == "-roads") {
            if (argc < 4) {
                std::cout << "错误: -show -roads 需要指定参数\n";
                return false;
            }

            std::string roads_param = argv[3];

            if (roads_param == "-all") {
                show_all_roads_grouped(p);
            }
            else if (roads_param == "-of") {
                if (argc < 5) {
                    std::cout << "错误: -show -roads -of 需要指定城市ID或名称\n";
                    return false;
                }
                try {
                    unsigned city_id = parse_town_id_or_name(p, argv[4]);
                    show_roads_of_city(p, city_id);
                }
                catch (const std::exception& e) {
                    std::cout << "错误: " << e.what() << std::endl;
                    return false;
                }
            }
            else if (roads_param == "-from") {
                if (argc < 5) {
                    std::cout << "错误: -show -roads -from 需要指定起点地点ID或名称\n";
                    return false;
                }
                try {
                    size_t from_id = parse_place_id_or_name(p, argv[4]);
                    show_roads_from_place(p, from_id);
                }
                catch (const std::exception& e) {
                    std::cout << "错误: " << e.what() << std::endl;
                    return false;
                }
            }
            else if (roads_param == "-to") {
                if (argc < 5) {
                    std::cout << "错误: -show -roads -to 需要指定终点地点ID或名称\n";
                    return false;
                }
                try {
                    size_t to_id = parse_place_id_or_name(p, argv[4]);
                    show_roads_to_place(p, to_id);
                }
                catch (const std::exception& e) {
                    std::cout << "错误: " << e.what() << std::endl;
                    return false;
                }
            }
            else {
                std::cout << "错误: 未知的 -roads 参数: " << roads_param << "\n";
                return false;
            }
        }
        else {
            std::cout << "错误: 未知的显示类型: " << show_type << "\n";
            return false;
        }
    }
    else if (command == "-clear") {
        // 在 Windows 上使用 cls 清屏
        system("cls");
    }
    else if (command == "-batch") {
        if (argc < 3) {
            std::cout << "错误: 缺少批处理文件路径\n";
            return false;
        }
        std::string file_path = argv[2];
        if (file_path.starts_with("\"") && file_path.ends_with("\"")) {
            file_path = file_path.substr(1, file_path.length() - 2);
        }

        std::ifstream file(file_path);
        if (!file.is_open()) {
            std::cout << "错误: 无法打开文件 " << file_path << std::endl;
            return false;
        }

        std::string line;
        int line_num = 0;
        while (std::getline(file, line)) {
            line_num++;
            if (line.empty() || line[0] == '#') continue;

            std::cout << "执行第 " << line_num << " 行: " << line << std::endl;

            std::vector<std::string> args;
            std::istringstream iss(line);
            std::string token;

            while (iss >> token) {
                args.push_back(token);
            }

            if (args.empty()) continue;

            std::vector<char*> cargs;
            cargs.push_back(argv[0]);
            for (auto& arg : args) {
                cargs.push_back(&arg[0]);
            }

            if (!parse_command(static_cast<int>(cargs.size()), cargs.data(), p)) {
                std::cout << "第 " << line_num << " 行执行失败\n";
            }
        }
        std::cout << "批处理执行完成，共执行 " << line_num << " 行\n";
    }
    else if (command == "-exit") {
        std::cout << "感谢使用，再见!\n";
        return false;
    }
    else {
        std::cout << "错误: 未知命令: " << command << "\n";
        print_help();
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    plat p;

    // 如果有命令行参数，执行命令后进入交互模式
    if (argc > 1) {
        if (!parse_command(argc, argv, p)) {
            return 1; // 如果命令执行失败或用户要求退出，直接返回
        }
        // 命令执行成功后，继续进入交互模式而不是退出
        std::cout << "命令执行完成，进入交互模式...\n";
    }

    // 交互模式
    std::cout << "欢迎使用地图导航系统!\n";
    std::cout << "输入 -help 查看可用命令\n";

    std::string input;
    while (true) {
        std::cout << "\n> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;

        std::vector<std::string> args;
        std::istringstream iss(input);
        std::string token;

        while (iss >> token) {
            args.push_back(token);
        }

        std::vector<char*> cargs;
        cargs.push_back(argv[0]);
        for (auto& arg : args) {
            cargs.push_back(&arg[0]);
        }

        if (!parse_command(static_cast<int>(cargs.size()), cargs.data(), p)) {
            if (args[0] == "-exit") {
                break;
            }
        }
    }

    return 0;
}
