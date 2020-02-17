//edit here
#define CURL_STATICLIB
#pragma comment(lib, "E:/Windows Kits/10/Lib/10.0.18362.0/um/x86/ws2_32.lib")
#pragma comment(lib, "E:/Windows Kits/10/Lib/10.0.18362.0/um/x86/crypt32.lib")
#pragma comment(lib, "E:/Windows Kits/10/Lib/10.0.18362.0/um/x86/Wldap32.lib")
#pragma comment(lib, "E:/Windows Kits/10/Lib/10.0.18362.0/um/x86/Normaliz.lib")
#pragma comment(lib, "D:/greensoft/curl-7.68.0/builds/libcurl-vc-x86-release-static-ipv6-sspi-winssl/lib/libcurl_a.lib")

#include <cqcppsdk/cqcppsdk.h>
#include <curl/curl.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include "nlohmann/json.hpp"

using namespace nlohmann;
using namespace cq;
using namespace std;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;
using cq::utils::base64_encode;
using cq::utils::base64_decode;

string path = "D:\\CFBot\\"; //edit here

string to_lower(string s) {
    for (auto& c : s) c = tolower(c);
    return s;
}
typedef long long ll;

string to_str(ll u) {
    char buf[100];
    sprintf(buf, "%lld", u);
    return buf;
}

bool possible_cf_handle(string u) {
    int l = u.size();
    if (l < 3 || l > 24) return 0;
    for (auto c : u)
        if (!(isdigit(c) || isalpha(c) || c == '_' || c == '-' || c == '.')) return 0;
    return 1;
}

struct duel_info {
    string winner, loser;
    pair<int, string> problem;
    int time;
    int accept;
};

struct user_info {
    string handle;
    bool silent;
    int point;
    vector<duel_info> vec;
};

map<ll, user_info> user_list;
map<string, ll> handles;

void to_json(json& j, const duel_info& p) {
    j = json{{"winner", p.winner}, {"loser", p.loser}, {"problem", p.problem}, {"time", p.time}};
}

void from_json(const json& j, duel_info& p) {
    j.at("winner").get_to(p.winner);
    j.at("loser").get_to(p.loser);
    j.at("problem").get_to(p.problem);
    j.at("time").get_to(p.time);
    p.accept = 0;
}

void to_json(json& j, const user_info& p) {
    j = json{{"handle", p.handle}, {"vec", p.vec}, {"silent", p.silent}, {"point", p.point}};
}

void from_json(const json& j, user_info& p) {
    j.at("handle").get_to(p.handle);
    j.at("vec").get_to(p.vec);
    j.at("silent").get_to(p.silent);
    j.at("point").get_to(p.point);
}

void save_user(ll w) {
    if (!user_list.count(w)) {
        remove((path + to_str(w) + ".json").c_str());
        throw invalid_argument("no such user!");
    }
    user_info s = user_list[w];
    for (int j = 1; j <= 5; ++j) {
        std::ofstream o(path + to_str(w) + ".json");
        if (o << json(s)) {
			o.close(); break;
		}
        o.close();
    }
}

vector<string> split_by_space(string u) {
    vector<string> o;
    string v;
    u = u + " ";
    for (auto c : u) {
        if (c == ' ') {
            if (v != "") o.push_back(v);
            v = "";
        } else
            v.push_back(c);
    }
    return o;
}
#include <filesystem>
namespace fs = std::filesystem;
map<ll, string> pending_bind;
map<ll, ll> pending_bind_t;

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string download_file(string url, int tl = 10) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;
    try {
        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, tl);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            return readBuffer;
        }
    } catch (...) {
    }
    return "";
}
json problems;
map<int, vector<pair<int, string>>> problem_difficulty;
map<pair<int, string>, int> diff_pro;
void refresh_problems() {
    ifstream t(path + "problemset.problems");
    std::string u((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
    /*
    string u = download_file("http://codeforces.com/api/problemset.problems", 100);
    if (u == "") {
        logging::info("错误", "刷新题目失败");
        return;
    }*/
    problems = json::parse(u);
    map<int, vector<pair<int, string>>> p2;
    int cnt = 0;
    for (auto t : problems["result"]["problems"]) {
        if (!t.count("rating")) continue;
        bool special = 0;
        for (auto s : t["tags"])
            if (s == "*special") special = 1;
        if (special) continue;
        ++cnt;
        diff_pro[make_pair((int)t["contestId"], (string)t["index"])] = t["rating"];
        p2[t["rating"]].push_back(make_pair((int)t["contestId"], (string)t["index"]));
    }
    problem_difficulty = p2;
    logging::info("成功", "刷新题目成功，获取了" + to_str(cnt) + "道题目");
}

int streak(ll qq) {
    auto& w = user_list[qq];
    int cw = 0;
    for (auto t : w.vec)
        if (t.winner == w.handle)
            ++cw;
        else
            cw = 0;
    return cw;
}

ll to_qq(string qq) {
    if (qq.find("[CQ:at") == 0) qq = qq.substr(qq.find("qq=") + 3, qq.size() - 2);
    qq = message::unescape(qq);
    if (qq.size() >= 2 && qq[0] == '[' && qq.back() == ']') {
        string u = qq.substr(1, qq.size() - 2);
        if (handles.count(to_lower(u))) return handles[to_lower(u)];
    }
    return atoll(qq.c_str());
}

vector<duel_info> duels;

bool in_duel(string u) {
    for (auto p : duels)
        if (p.loser == u || p.winner == u) return 1;
    return 0;
}
bool in_duel(ll t) {
    return in_duel(user_list[t].handle);
}
string pe() {
    string g = download_file("https://projecteuler.net/news");
    g = g.substr(g.find("<ul style="));
    g = g.substr(0, g.find("</ul>"));
    string w = "";
    int ig = 0;
    for (auto c : g) {
        if (c == '<') {
            ig = 1;
            if (w.empty() || w[w.size() - 1] == '\n')
                ;
            else
                w.push_back('\n');
        } else if (c == '>')
            ig = 0;
        else if (!ig) {
            if (c == ' ' && (w.empty() || w[w.size() - 1] == ' '))
                ;
            else
                w.push_back(c);
        }
    }
    while (w.size() && (w.back() == ' ' || w.back() == '\n')) w.pop_back();
    int j = -1, k1, k2;
    while (1) {
        k1 = w.find("am", j + 1);
        k2 = w.find("pm", j + 1);
        if (k1 == -1 && k2 == -1) break;
        if (k1 == -1) k1 = 1e9;
        if (k2 == -1) k2 = 1e9;
        j = min(k1, k2);
        int c = j;
        while (c > 0 && w[c] != ',') --c;
        --c;
        while (c > 0 && w[c] != ',') --c;
        c += 2;
        string u = w.substr(c, j - c + 2);
        u.erase(u.begin() + u.find(' ') - 1);
        u.erase(u.begin() + u.find(' ') - 1);
        while (u[u.find(' ') + 4] != ' ') u.erase(u.begin() + u.find(' ') + 4);
        std::tm t = {};
        std::istringstream ss(u);
        ss.imbue(std::locale("en_US.utf8"));
        ss >> std::get_time(&t, "%d %b %Y, %R %p");
        auto uu = std::mktime(&t) + 8 * 3600;
        std::tm* ptm = std::localtime(&uu);
        char buffer[32];
        std::strftime(buffer, 32, "%Y/%d/%m %H:%M:%S", ptm);
        w.insert(j + 2, " [UTC+8: " + string(buffer) + "]");
    }
    return w;
}

string problem_desc(pair<int, string> pro) {
    return to_str(pro.first) + pro.second + " *" + to_str(diff_pro[pro]);
}

std::mt19937 rng(time(0) + clock());

CQ_INIT {
    on_enable([] {
        refresh_problems();
        for (const auto& entry : fs::directory_iterator(path)) {
            string u = entry.path().u8string();
            string v = entry.path().filename().u8string();
            if (v.size() < 5 || v.substr(v.size() - 5) != ".json") continue;
            ifstream t(u);
            std::string str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
            ll uid = atoll(v.substr(0, v.size() - 5).c_str());
            user_info ui = json::parse(str);
            user_list[uid] = ui;
            handles[to_lower(ui.handle)] = uid;
        }
        logging::info("启用", "成功读取" + to_str(user_list.size()) + "条数据");
    });

    on_private_message([](const PrivateMessageEvent& e) {});

    on_message([](const MessageEvent& e) {});

    on_group_message([](const GroupMessageEvent& e) {
		//edit here
        static const set<ll> admin_list = {1014768217};
        static const set<ll> enabled_groups = {1053740640, 617894285};
        if (!enabled_groups.count(e.group_id)) return;
        if (e.message.substr(0, 1) != ";") return;
        if (e.is_anonymous()) return;
        bool is_admin = admin_list.count(e.user_id);
        string msg = e.message.substr(1);
        vector<string> msg_vec = split_by_space(msg);
        try {
            string op = msg_vec.at(0);
            if (op == "pe") {
                send_group_message(e.group_id, pe());
            } else if (op == "bind") {
                if (user_list.count(e.user_id)) {
                    send_group_message(e.group_id, "您已经绑定Codeforces账号");
                } else if (pending_bind.count(e.user_id)) {
                    string hd = pending_bind[e.user_id];
                    if (handles.count(to_lower(hd))) {
                        send_group_message(e.group_id, "该Codeforces账号已被绑定，请重新开始绑定");
                        throw invalid_argument("used handle");
                    }
                    pending_bind.erase(e.user_id);
                    if (user_list.count(e.user_id)) throw invalid_argument("already in user list");
                    string rt =
                        download_file("http://codeforces.com/api/user.status?handle=" + hd + "&from=1&count=10");
                    if (rt == "") {
                        send_group_message(e.group_id, "绑定失败，未知错误，请重新开始绑定");
                        throw invalid_argument("invalid");
                    }
                    json w = json::parse(rt);
                    if (w["status"] != "OK") {
                        send_group_message(e.group_id, "绑定失败，未知错误，请重新开始绑定");
                        throw invalid_argument("invalid");
                    }
                    bool success = 0;
                    for (auto t : w["result"]) {
                        if (t["problem"]["name"] == "Theatre Square") {
                            long long w = t["creationTimeSeconds"] - pending_bind_t[e.user_id];
                            if (w <= 65 && w >= 0) success = 1;
                        }
                    }
                    if (!success) {
                        send_group_message(e.group_id, "绑定失败，未找到合法提交，请重新开始绑定");
                        throw invalid_argument("invalid");
                    }
                    user_info u;
                    u.handle = hd;
                    u.point = 1000;
                    u.silent = true;
                    user_list[e.user_id] = u;
                    handles[to_lower(hd)] = e.user_id;
                    save_user(e.user_id);
                    send_group_message(e.group_id, "您已成功绑定Codeforces账号");
                } else {
                    string ou = msg_vec.at(1);
                    if (!possible_cf_handle(ou)) throw invalid_argument("should be valid codeforces handle");
                    if (handles.count(to_lower(ou))) {
                        send_group_message(e.group_id, "该Codeforces账号已被绑定");
                        throw invalid_argument("used handle");
                    }
                    pending_bind[e.user_id] = ou;
                    pending_bind_t[e.user_id] = time(0);
                    send_group_message(e.group_id,
									   "正在为 [CQ:at,qq=" + to_str(e.user_id) + "] 绑定账号 [" + ou
										   + "]，请注意一旦绑定成功后无法解绑。请在一分钟内提交一个Compile Error：https://codeforces.com/problemset/problem/1/A ，并输入;bind");
                }
            } else if (op == "get") {
                string qq = msg_vec.at(1);
                long long s = to_qq(qq);
                if (!user_list.count(s))
                    send_group_message(e.group_id, "未找到 " + to_str(s) + " 所绑定的Codeforces账号");
                else {
                    auto& r = user_list[s];
                    send_group_message(e.group_id,
                                       to_str(s) + " 绑定的Codeforces账号为 [" + r.handle
                                           + "] https://codeforces.com/profile/" + r.handle);
                }
            } else if (op == "duel") {
                string op2 = msg_vec.at(1);
                if (op2 == "history") {
                    ll ppl = (msg_vec.size() >= 3) ? to_qq(msg_vec[2]) : e.user_id;
                    if (!user_list.count(ppl)) {
                        send_group_message(e.group_id, "该qq号不存在或未绑定Codeforces账号");
                        throw invalid_argument("");
                    }
                    string hd = user_list[ppl].handle;
                    auto& t = user_list[ppl].vec;
                    int win = 0, lose = 0;
                    for (auto u : t)
                        if (u.winner == hd)
                            ++win;
                        else
                            ++lose;
                    string msg = to_str(ppl) + " [" + hd + "] 胜" + to_str(win) + " 负" + to_str(lose) + "\n";
                    msg += "当前连胜" + to_str(streak(ppl)) + " 积分" + to_str(user_list[ppl].point);
                    if (t.size()) {
                        for (auto u = t.rbegin(); u != t.rend(); ++u)
                            msg = msg + "\n[" + u->winner + "] 打败了 [" + u->loser + "] " + problem_desc(u->problem)
                                  + " (" + to_str(u->time) + "s)";
                    }
                    send_group_message(e.group_id, msg);
                    throw invalid_argument("");
                } else if (op2 == "ongoing") {
                    auto v = duels;
                    vector<duel_info> vt;
                    std::shuffle(v.begin(), v.end(), rng);
                    int t0 = 0, t1 = 0;
                    for (auto c : v) {
                        if (c.accept)
                            ++t1, vt.push_back(c);
                        else
                            ++t0;
                    }
                    string ut = "当前有" + to_str(t0) + "个未接受的对战和" + to_str(t1) + "个进行中的对战。";
                    for (auto w : vt) {
                        ut += "\n进行中：[" + w.winner + "] vs [" + w.loser + "] " + to_str(time(0) - w.time) + "s";
                    }
                    send_group_message(e.group_id, ut);
                    throw invalid_argument("");
                } else if (op2 == "list") {
                    vector<pair<ll, string>> vs;
                    for (auto t : user_list)
                        if (!t.second.silent) {
                            vs.push_back(make_pair(t.first, t.second.handle));
                        }
                    string ut = "当前有" + to_str(vs.size()) + "位接受对战的成员。";
                    for (auto t : vs) ut += "\n" + to_str(t.first) + " [" + t.second + "]";
                    send_group_message(e.group_id, ut);
                    throw invalid_argument("");
                }
                if (!user_list.count(e.user_id)) {
                    send_group_message(e.group_id, "请先绑定Codeforces账号");
                    throw invalid_argument("");
                }
                ll cq = e.user_id;
                if (op2 == "toggle") {
                    auto& t = user_list[e.user_id].silent;
                    if (!t)
                        send_group_message(e.group_id, "已为您暂时关闭迎战功能");
                    else
                        send_group_message(e.group_id, "已为您重新开启迎战功能");
                    t ^= 1;
                    save_user(e.user_id);
                } else if (op2 == "challenge") {
                    string op = msg_vec.at(2);
                    ll opq = to_qq(op);
                    if (!user_list.count(opq)) {
                        send_group_message(e.group_id, "未找到 " + to_str(opq) + " 绑定的Codeforces账号");
                        throw invalid_argument("");
                    }
                    if (opq == cq) {
                        send_group_message(e.group_id, "无法自己挑战自己");
                        throw invalid_argument("");
                    }
                    if (in_duel(cq)) {
                        send_group_message(e.group_id, "您已在对战中");
                        throw invalid_argument("");
                    }
                    if (in_duel(opq)) {
                        send_group_message(e.group_id, "对方已在对战中");
                        throw invalid_argument("");
                    }
                    if (user_list[opq].silent) {
                        send_group_message(e.group_id, "对方目前关闭了迎战功能");
                        throw invalid_argument("");
                    }
                    ll diff = atoll(msg_vec.at(3).c_str());
                    auto& vec = problem_difficulty[diff];
                    if (vec.size() < 10) {
                        send_group_message(e.group_id, "难度为" + to_str(diff) + "的题目不存在或过少");
                    } else {
                        pair<int, string> u = vec[uniform_int_distribution<>(0, vec.size() - 1)(rng)];
                        duel_info t;
                        t.accept = 0;
                        t.winner = user_list[cq].handle;
                        t.loser = user_list[opq].handle;
                        t.problem = u;
                        duels.push_back(t);
                        send_group_message(e.group_id,
                                           "[CQ:at,qq=" + to_str(cq) + "]向[CQ:at,qq=" + to_str(opq)
                                               + "]发起了挑战，题目难度为" + to_str(diff)
                                               + "！输入;duel accept以接受，;duel decline以退出。");
                    }
                } else if (op2 == "decline") {
                    if (!in_duel(cq)) {
                        send_group_message(e.group_id, "您不在对战中");
                        throw invalid_argument("");
                    }
                    string ch = user_list[cq].handle;
                    for (auto t = duels.begin(); t != duels.end(); ++t) {
                        if (t->loser == ch || t->winner == ch) {
                            send_group_message(e.group_id,
                                               "已为您退出当前对战[CQ:at,qq=" + to_str(handles[to_lower(t->loser)])
                                                   + "][CQ:at,qq=" + to_str(handles[to_lower(t->winner)]) + "]");
                            duels.erase(t);
                            break;
                        }
                    }
                } else if (op2 == "accept") {
                    if (!in_duel(cq)) {
                        send_group_message(e.group_id, "您不在对战中");
                        throw invalid_argument("");
                    }
                    string ch = user_list[cq].handle;
                    for (auto t = duels.begin(); t != duels.end(); ++t) {
                        if (t->loser == ch || t->winner == ch) {
                            if (t->accept)
                                send_group_message(e.group_id, "当前对战已开始");
                            else if (t->loser == ch) {
                                t->accept = 1;
                                t->time = time(0);
                                send_group_message(e.group_id,
                                                   "[CQ:at,qq=" + to_str(handles[to_lower(t->winner)])
                                                       + "]与[CQ:at,qq=" + to_str(handles[to_lower(t->loser)])
                                                       + "]的对战开始了！如果你见过这道题目，可以输入;duel decline");
                                send_group_message(e.group_id,
                                                   "https://codeforces.com/problemset/problem/"
                                                       + to_str(t->problem.first) + "/" + t->problem.second);
                            } else
                                send_group_message(e.group_id, "您为当前对战发起者");
                            break;
                        }
                    }
                } else if (op2 == "check") {
                    if (!in_duel(cq)) {
                        send_group_message(e.group_id, "您不在对战中");
                        throw invalid_argument("");
                    }
                    string ch = user_list[cq].handle;
                    for (auto t = duels.begin(); t != duels.end(); ++t) {
                        if (t->loser == ch || t->winner == ch) {
                            if (!t->accept) {
                                send_group_message(e.group_id, "当前对战未开始");
                                throw invalid_argument("");
                            }
                            string p1 = t->winner, p2 = t->loser;
                            bool pend1 = false, pend2 = false;
                            ll inf = 1e18;
                            ll t1 = inf, t2 = inf;
                            ll tu1 = inf, tu2 = inf;
                            string o1 =
                                download_file("http://codeforces.com/api/user.status?handle=" + p1 + "&count=10");
                            if (o1 == "") {
                                send_group_message(e.group_id, "网络错误");
                                throw invalid_argument("");
                            }
                            string o2 =
                                download_file("http://codeforces.com/api/user.status?handle=" + p2 + "&count=10");
                            if (o2 == "") {
                                send_group_message(e.group_id, "网络错误");
                                throw invalid_argument("");
                            }
                            json w1 = json::parse(o1), w2 = json::parse(o2);
                            if (w1["status"] != "OK" || w2["status"] != "OK") {
                                send_group_message(e.group_id, "网络错误");
                                throw invalid_argument("");
                            }
                            auto cp = t->problem;
                            auto tm = t->time;
                            for (auto t : w1["result"]) {
                                auto pg = make_pair((int)t["problem"]["contestId"], (string)t["problem"]["index"]);
                                if (pg != cp) continue;
                                if (t["creationTimeSeconds"] < tm) continue;
                                if (t["verdict"] == "OK") t1 = min(t1, t["creationTimeSeconds"]);
                                if (t["verdict"] == "TESTING") pend1 = true, tu1 = min(tu1, t["creationTimeSeconds"]);
                            }
                            for (auto t : w2["result"]) {
                                auto pg = make_pair((int)t["problem"]["contestId"], (string)t["problem"]["index"]);
                                if (pg != cp) continue;
                                if (t["creationTimeSeconds"] < tm) continue;
                                if (t["verdict"] == "OK") t2 = min(t2, t["creationTimeSeconds"]);
                                if (t["verdict"] == "TESTING") pend2 = true, tu2 = min(tu2, t["creationTimeSeconds"]);
                            }
                            tu1 = min(tu1, t1);
                            tu2 = min(tu2, t2);
                            if (t1 == inf && t2 == inf) {
                                send_group_message(e.group_id, "尚未有人通过");
                                throw invalid_argument("");
                            }
                            if (t1 < tu2) {
                                t->time = t1 - t->time;
                            } else if (t2 < tu1 || t1 == t2) {
                                t->time = t2 - t->time;
                                swap(t->winner, t->loser);
                            } else {
                                send_group_message(e.group_id, "仍有评测在进行中");
                                throw invalid_argument("");
                            }
                            string msg = "[" + t->winner + "]胜利了！用时" + to_str(t->time)
                                         + "s！[CQ:at,qq=" + to_str(handles[to_lower(t->winner)])
                                         + "][CQ:at,qq=" + to_str(handles[to_lower(t->loser)]) + "]";
                            auto &u1 = user_list[handles[to_lower(t->winner)]],
                                 &u2 = user_list[handles[to_lower(t->loser)]];
                            int pu1 = u1.point, pu2 = u2.point;
                            vector<pair<string, int>> d1, d2;
                            int diff = diff_pro[t->problem];
                            d1.push_back({"胜利", 50});
                            d2.push_back({"失败", -50});
                            if (diff / 100) d1.push_back({"难度奖励", diff / 100});
                            int rn = rand() % 5 + 1, tb = 0;
                            int tt = max(t->time, 1);
                            if (tt <= 600) tb = min(int(log(600.0 / tt) * 10 + 0.5), 30);
                            int s1 = streak(handles[to_lower(t->loser)]);
                            u1.vec.push_back(*t);
                            u2.vec.push_back(*t);
                            int s2 = streak(handles[to_lower(t->winner)]);
                            if (s2 >= 2) d1.push_back({"连胜奖励", (s2 - 1) * 10});
                            if (s1 >= 2) d1.push_back({"终结连胜", (s1 - 1) * 10});
                            if (tb) d1.push_back({"时间奖励", tb});
                            d1.push_back({"随机奖励", rn});
                            d2.push_back({"随机奖励", rn});
                            msg += "\n[" + t->winner + "]";
                            for (auto t : d1) u1.point += t.second;
                            u1.point = max(u1.point, 0);
                            msg += " " + to_str(pu1) + "->" + to_str(u1.point) + "\n  ";
                            for (auto t : d1) {
                                if (!t.second) continue;
                                msg += " " + t.first;
                                if (t.second > 0)
                                    msg += "+";
                                else
                                    msg += "-";
                                msg += to_str(abs(t.second));
                            }
                            msg += "\n[" + t->loser + "]";
                            for (auto t : d2) u2.point += t.second;
                            u2.point = max(u2.point, 0);
                            msg += " " + to_str(pu2) + "->" + to_str(u2.point) + "\n  ";
                            for (auto t : d2) {
                                if (!t.second) continue;
                                msg += " " + t.first;
                                if (t.second > 0)
                                    msg += "+";
                                else
                                    msg += "-";
                                msg += to_str(abs(t.second));
                            }
                            send_group_message(e.group_id, msg);
                            duels.erase(t);
                            save_user(handles[to_lower(t->winner)]);
                            save_user(handles[to_lower(t->loser)]);
                            break;
                        }
                    }
                }
            } /*else if (op == "pro") {
                string u = msg_vec.at(1);
                if (u.substr(0, 2) == "CF") {
                    string p = u.substr(2);
                    string v = "";
                    while (p.size() && isdigit(p[0])) v.push_back(p[0]), p.erase(p.begin());
                    int a = atoi(v.c_str());
                    send_group_message(e.group_id,
                                       "https://codeforces.com/problemset/problem/" + to_str(a) + "/" + p);
                }
            } */
            else if (op == "help") {
                string help =
                    "------------------CF对战机器人------------------\n"
                    "提示：输入qq号或@表示对应qq，codeforces账号需要用[]包围，如;get [xxx]\n"
                    ";help - 显示帮助\n"
                    ";bind [codeforces账号] - 绑定codeforces账号\n"
                    ";get [qq号/codeforces账号] - 查询某个账号的对应信息\n"
                    ";duel list - 查询接受对战的成员列表\n"
                    ";duel toggle - 切换是否接受对战（默认为否）\n"
                    ";duel challenge [qq号/codeforces账号] [难度] - 发起对战\n"
                    ";duel accept - 接受对战\n"
                    ";duel decline - 取消对战\n"
                    ";duel check - 查看对战是否结束（建议在AC后执行）\n"
                    ";duel ongoing - 查看当前进行中的对战\n"
                    ";duel history [qq号/codeforces账号] - 查看某个账号的对战历史\n"
                    //                    ";pro CF[codeforces题目编号] - 获取题目链接\n"
                    ";pe - Project Euler 放题时间";
                if (is_admin)
                    help +=
                        "\n"
                        "------------------以下命令需要权限------------------\n"
                        ";refresh - 刷新题目列表（需要人工操作）\n"
                        ";forcebind - 强制绑定\n"
                        ";forceunbind - 强制解绑";
                send_group_message(e.group_id, help);
            } else if (is_admin) {
                if (op == "refresh") {
                    send_group_message(e.group_id, "开始刷新题目列表");
                    refresh_problems();
                } else if (op == "forcebind") {
                    ll q = to_qq(msg_vec.at(1));
                    string hd = msg_vec.at(2);
                    user_info u;
                    u.handle = hd;
                    u.silent = true;
                    user_list[q] = u;
                    handles[to_lower(hd)] = e.user_id;
                    save_user(q);
                    send_group_message(e.group_id, "已成功为" + to_str(q) + " [" + hd + "]绑定Codeforces账号");
                } else if (op == "forceunbind") {
                    ll q = to_qq(msg_vec.at(1));
                    if (!user_list.count(q)) {
                        send_group_message(e.group_id, "该用户不存在或未绑定Codeforces号");
                    } else {
                        string hd = user_list[q].handle;
                        handles.erase(to_lower(hd));
                        user_list.erase(q);
                        send_group_message(e.group_id, "已成功解绑 " + to_str(q) + " [" + hd + "]");
                        save_user(q);
                    }
                }
            }
        } catch (...) {
        }
        e.block();
    });

    on_group_upload([](const auto& e) {});
}