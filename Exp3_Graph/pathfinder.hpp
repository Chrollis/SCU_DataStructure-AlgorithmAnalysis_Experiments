#ifndef PATHFINDER_HPP
#define PATHFINDER_HPP

#include <iostream>
#include <list>
#include <queue>
#include <cmath>
#include <unordered_map>
#include <stdexcept>
#include <unordered_set>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <functional>

namespace chr {

    // 将距离（米）转换成人可读字符串（根据大小选择单位）
    std::string distance_to_string(double distance);

    // 简单二维点类型：用于经纬度平面坐标（UTM）与原始经纬数据的封装
    class point2d {
    private:
        double x_, y_;
    public:
        point2d() :x_(0), y_(0) {}
        point2d(double x, double y) :x_(x), y_(y) {}
        double x() const { return x_; }
        double y() const { return y_; }
        double& rx() { return x_; }
        double& ry() { return y_; }
        point2d operator+(const point2d& other) const { return { x_ + other.x_,y_ + other.y_ }; }
        point2d operator-(const point2d& other) const { return { x_ - other.x_,y_ - other.y_ }; }
        point2d operator*(double k) const { return { x_ * k,y_ * k }; }
        point2d operator/(double k) const { return { x_ / k,y_ / k }; }
        bool operator==(const point2d& other) const { return x_ == other.x_ && y_ == other.y_; }
        bool operator>(const point2d& other) const { return x_ > other.x_ || y_ > other.y_; }
        bool operator<(const point2d& other) const { return x_ < other.x_ || y_ < other.y_; }
        bool operator!=(const point2d& other) const { return !(*this == other); }
        bool operator>=(const point2d& other) const { return !(*this < other); }
        bool operator<=(const point2d& other) const { return !(*this > other); }
        double dot(const point2d& other) const { return x_ * other.x_ + y_ * other.y_; }
        double norm() const { return std::sqrt(x_ * x_ + y_ * y_); }
        double distance_to(const point2d& other) const { return (other - *this).norm(); }
        point2d unit_vector() const { return *this / norm(); }
    };

    // 位置信息（地点）
    // - id_: 全局唯一的地点标识：高 32 位为城市 id，低 32 位为城市内序列号（place_serial）
    // - name_: 地点名称
    // - globe_: 存储原始经纬（注意：本项目以 (lat, lon) 的顺序存储在 point2d 中）
    // - plane_: 对应的平面坐标（UTM），用于计算线路距离和 A* 启发式
    // - roads_: 邻接表：key 为目标地点 id，value 为在 plane_ 坐标系下的距离（米）
    class location {
    public:
        // 根据经度选择 UTM 带（整带编号）
        static int utm_zone(double lon) { return static_cast<int>((lon + 180.0) / 6.0) + 1; }
        // 将 WGS84 经纬（lon, lat）转换为 UTM 平面坐标（返回 point2d(x=easting, y=northing)）
        static point2d wgs84_to_utm(double lon, double lat);
    private:
        size_t id_;
        std::string name_;
        point2d globe_;
        point2d plane_;
        std::unordered_map<size_t, double> roads_;
    public:
        location() :id_(0) {}
        location(size_t id, const std::string& name, const point2d& globe_coordinate);
        size_t id() const { return id_; }
        const std::string& name() const { return name_; }
        void rename(const std::string& new_name) { name_ = new_name; }
        point2d globe() const { return globe_; }
        double longitude() const { return globe_.y(); }
        double latitude() const { return globe_.x(); }
        point2d plane() const { return plane_; }
        const std::unordered_map<size_t, double>& roads() const { return roads_; }
        // 添加一条从当前地点到 id 的边，计算并保存平面坐标距离
        void add_road(size_t id, const point2d& plane_coordinate) { roads_[id] = plane_.distance_to(plane_coordinate); }
        // 删除道路（返回是否删除成功）
        bool remove_road(size_t id) {return roads_.erase(id) > 0;}
        bool has_road_to(size_t id) const {return roads_.find(id) != roads_.end();}
        // 返回到指定地点的路长，否则 0.0
        double road_length_to(size_t id) const;
    };

    // 城市（town）
    // 负责管理同一城市内的地点集合（places_）与城市级别的道路添加/查询
    class city {
    public:
        // 将城市 id 与城市内部序列号组合为全局 place id（高 32 位：city_id，低 32 位：place_serial）
        static size_t place_id(unsigned city_id, unsigned place_serial) { return ((size_t)(city_id) << 32) + place_serial; }
    private:
        unsigned id_;
        std::string name_;
        std::unordered_map<size_t, std::shared_ptr<location>> places_;
    public:
        city() :id_(0) {}
        city(unsigned id, const std::string& name);
        size_t id() const { return id_; }
        const std::string& name() const { return name_; }
        const std::unordered_map<size_t, std::shared_ptr<location>>& places() const { return places_; }
        bool has_place(size_t id) const { return places_.find(id) != places_.end(); }
        bool has_local_place(unsigned serial) const { return places_.find(place_id(id_, serial)) != places_.end(); }
        std::shared_ptr<location> place(size_t id) const;
        std::shared_ptr<location> local_place(unsigned serial) const;
        // 添加地点（全局 id）
        location& add_place(size_t id, const std::string& name, const point2d& globe_coordinate);
        // 添加城市内地点（以序列号）
        location& add_local_place(unsigned serial, const std::string& name, const point2d& globe_coordinate);
        bool remove_place(size_t id);
        bool remove_local_place(unsigned serial);
        void rename(const std::string& new_name);
        bool rename_place(size_t id, const std::string& new_name);
    public:
        // 城市内部/跨城市道路管理接口（高层封装）
        double add_road(size_t from, size_t to) const;
        double add_local_road(unsigned from_serial, unsigned to_serial) const;
        double add_bidirectional_road(size_t from, size_t to) const;
        double add_local_bidirectional_road(unsigned from_serial, unsigned to_serial) const;
        // 从城市内部向外部添加道路，plane_coordinate 为目标地点在平面坐标下的位置
        double add_intercity_road(size_t from, size_t to, const point2d& plane_coordinate) const;
        bool has_road(size_t from, size_t to) const;
        bool has_local_road(unsigned from_serial, unsigned to_serial) const;
        double road_length(size_t from, size_t to) const;
        double local_road_length(unsigned from_serial, unsigned to_serial) const;
    };

    // 平台（plat）: 整个地图的数据结构与搜索接口
    // - 管理所有城市（towns_）
    // - 提供 A* 路径搜索、模糊查找、JSON 导入导出等功能
    class plat {
    private:
        std::unordered_map<unsigned, std::shared_ptr<city>> towns_;
        struct astar_node {
            double g;
            double f;
            size_t parent;
            bool operator>(const astar_node& other) const { return f > other.f; }
            bool operator==(const astar_node& other) const { return g == other.g && f == other.f && parent == other.parent; }
            // 启发式函数：使用平面坐标的欧氏距离
            static double heuristic(const point2d& a, const point2d& b) { return a.distance_to(b); }
        };
    public:
        const std::unordered_map<unsigned, std::shared_ptr<city>>& towns() const { return towns_; }
        std::shared_ptr<location> place(size_t id) const;
        city& add_town(unsigned id, const std::string& name);
        bool has_town(unsigned id) { return towns_.find(id) != towns_.end(); }
        std::shared_ptr<city> town(unsigned id) const;
        bool remove_town(unsigned id);
        bool rename_town(unsigned id, const std::string& new_name);
        bool rename_place(size_t id, const std::string& new_name);
        std::vector<unsigned> get_all_town_ids() const;
        // A* 查找并返回地点 id 的路径（若无路径返回空向量）
        std::vector<size_t> find_path(size_t from, size_t to) const;
        // 将路径以可读格式输出（含路段长度和方向信息）
        void print_path(const std::vector<size_t>& path) const;
    public:
        // 模糊查找地点与城市（返回 id 与显示用名称）
        std::vector<std::pair<size_t, std::string>> fuzzy_find_places(const std::string& keyword) const;
        std::vector<std::pair<unsigned, std::string>> fuzzy_find_towns(const std::string& keyword) const;
        // 道路管理（高层）
        double add_road(size_t from, size_t to) const;
        double add_bidirectional_road(size_t from, size_t to) const;
        bool has_road(size_t from, size_t to) const;
        double road_length(size_t from, size_t to) const;
    public:
        // JSON 序列化/反序列化（简单实现，按当前格式读写）
        void save_all_cities_as_json(const std::filesystem::path& path) const;
        void load_all_cities_from_json(const std::filesystem::path& path);
    private:
        // A* 实现细节：以地点的 plane 坐标作为搜索空间，返回 id 序列
        std::vector<size_t> astar_search(const location& start, const location& goal) const;
        std::vector<size_t> reconstruct_path(const std::unordered_map<size_t, astar_node>& nodes, size_t end_id) const;
    };
}

#endif // !PATHFINDER_HPP
