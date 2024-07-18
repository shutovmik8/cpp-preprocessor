#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

int line = 1;
path begin_file;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

string GetFileContents(string file) {
    ifstream stream(file);
    return {(istreambuf_iterator<char>(stream)), istreambuf_iterator<char>()};
}

bool Preprocess_work(const string& name, ofstream& out, const vector<path>& include_directories, const path& parent_file, bool search_this_dir) {
    ifstream in;
    path found_dir;
    if (search_this_dir) {
        found_dir = parent_file.parent_path() / path(name);
        in.open(found_dir);
    }

    if (!in.is_open()) {
        for (const auto& p : include_directories) {
            path end_path = p / path(name);
            in.open(end_path);
            if (in.is_open()) {
                found_dir = end_path;
                break;
            }
        }
    }

    if (!in.is_open()) {
        cout << "unknown include file " << name << " at file " << parent_file.string() << " at line " << line << endl;
        return false;
    }

    static regex reg1(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static regex reg2(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    string file_str;
    smatch m;
    while (getline(in, file_str)) {
        if (regex_match(file_str, m, reg1)) {
            if (!Preprocess_work(m[1], out, include_directories, found_dir, true)) {
                return false;
            }
            if (found_dir == begin_file) {
                ++line;
            }
            continue;
        } 
        else if (regex_match(file_str, m, reg2)) {
            if (!Preprocess_work(m[1], out, include_directories, found_dir, false)) {
                return false;
            }
            if (found_dir == begin_file) {
                ++line;
            }
            continue;
        }

        out.write(file_str.data(), file_str.size());
        out.put('\n');
        if (found_dir == begin_file) {
            ++line;
        }
    }
    in.close();
    return true;
}

bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream in(in_file);
    if (!in.is_open()) {
        return false;
    }
    begin_file = in_file;
    in.close();

    ofstream out(out_file);
    bool ret = Preprocess_work(in_file.filename().string(), out, include_directories, in_file, true);
    out.close();
    return ret;
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                                  {"sources"_p / "include1"_p,"sources"_p / "include2"_p})));

    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}