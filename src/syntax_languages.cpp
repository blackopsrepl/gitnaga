#include "syntax_highlight.hpp"

#include <QFileInfo>
#include <QHash>

namespace GitNaga {

namespace {

QSet<QString> words(const QString &list)
{
    const auto parts = list.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return { parts.begin(), parts.end() };
}

LanguageRules cFamily(const QString &name, const QString &keywords, const QString &types)
{
    LanguageRules rules;
    rules.name = name;
    rules.keywords = words(keywords);
    rules.types = words(types);
    rules.constants = words(QStringLiteral("true false nullptr NULL"));
    rules.lineComments = { QStringLiteral("//") };
    rules.blockOpen = QStringLiteral("/*");
    rules.blockClose = QStringLiteral("*/");
    return rules;
}

LanguageRules hashFamily(const QString &name, const QString &keywords, const QString &constants)
{
    LanguageRules rules;
    rules.name = name;
    rules.keywords = words(keywords);
    rules.constants = words(constants);
    rules.lineComments = { QStringLiteral("#") };
    return rules;
}

const QString kCppKeywords = QStringLiteral(
    "alignas alignof auto break case catch class concept const consteval constexpr constinit const_cast continue "
    "co_await co_return co_yield decltype default delete do dynamic_cast else enum explicit export extern final "
    "for friend goto if inline mutable namespace new noexcept operator override private protected public "
    "reinterpret_cast requires return sizeof static static_assert static_cast struct switch template this "
    "thread_local throw try typedef typeid typename union using virtual volatile while emit signals slots");
const QString kCppTypes = QStringLiteral(
    "bool char char8_t char16_t char32_t double float int long short signed unsigned void wchar_t size_t "
    "int8_t int16_t int32_t int64_t uint8_t uint16_t uint32_t uint64_t qint64 qsizetype quint8 qreal");
const QString kJsKeywords = QStringLiteral(
    "async await break case catch class const continue debugger default delete do else export extends finally "
    "for from function if import in instanceof let new of return static super switch this throw try typeof var "
    "void while with yield as interface type enum implements declare namespace readonly keyof abstract");

QHash<QString, LanguageRules> buildLanguages()
{
    QHash<QString, LanguageRules> table;
    auto cpp = cFamily(QStringLiteral("C++"), kCppKeywords, kCppTypes);
    cpp.preprocessor = true;
    cpp.capitalizedTypes = true;
    table.insert(QStringLiteral("cpp"), cpp);

    auto rust = cFamily(QStringLiteral("Rust"),
                        QStringLiteral("as async await break const continue crate dyn else enum extern fn for if impl in "
                                       "let loop match mod move mut pub ref return self Self static struct super trait "
                                       "type unsafe use where while"),
                        QStringLiteral("bool char f32 f64 i8 i16 i32 i64 i128 isize str u8 u16 u32 u64 u128 usize"));
    rust.constants = words(QStringLiteral("true false None Some Ok Err"));
    rust.capitalizedTypes = true;
    rust.quotes = QStringLiteral("\"");
    table.insert(QStringLiteral("rust"), rust);

    auto go = cFamily(QStringLiteral("Go"),
                      QStringLiteral("break case chan const continue default defer else fallthrough for func go goto if "
                                     "import interface map package range return select struct switch type var"),
                      QStringLiteral("bool byte complex64 complex128 error float32 float64 int int8 int16 int32 int64 "
                                     "rune string uint uint8 uint16 uint32 uint64 uintptr any"));
    go.constants = words(QStringLiteral("true false nil iota"));
    go.quotes = QStringLiteral("\"'`");
    table.insert(QStringLiteral("go"), go);

    auto js = cFamily(QStringLiteral("JavaScript"), kJsKeywords,
                      QStringLiteral("string number boolean any unknown never object symbol bigint"));
    js.constants = words(QStringLiteral("true false null undefined NaN Infinity"));
    js.quotes = QStringLiteral("\"'`");
    js.capitalizedTypes = true;
    js.decorators = true;
    table.insert(QStringLiteral("js"), js);

    auto qml = js;
    qml.name = QStringLiteral("QML");
    qml.keywords.unite(words(QStringLiteral("property readonly required signal alias component pragma on")));
    qml.types.unite(words(QStringLiteral("int real double bool string var color url list point rect size")));
    table.insert(QStringLiteral("qml"), qml);

    auto python = hashFamily(QStringLiteral("Python"),
                             QStringLiteral("and as assert async await break class continue def del elif else except "
                                            "finally for from global if import in is lambda nonlocal not or pass raise "
                                            "return try while with yield match case self"),
                             QStringLiteral("True False None"));
    python.types = words(QStringLiteral("int float str bytes bool list dict set tuple object type"));
    python.tripleQuotes = true;
    python.decorators = true;
    python.capitalizedTypes = true;
    table.insert(QStringLiteral("python"), python);

    auto ruby = hashFamily(QStringLiteral("Ruby"),
                           QStringLiteral("alias and begin break case class def defined? do else elsif end ensure for "
                                          "if in module next not or redo rescue retry return self super then undef "
                                          "unless until when while yield require require_relative attr_reader "
                                          "attr_accessor private protected"),
                           QStringLiteral("true false nil"));
    ruby.capitalizedTypes = true;
    ruby.sigils = true;
    table.insert(QStringLiteral("ruby"), ruby);

    auto shell = hashFamily(QStringLiteral("Shell"),
                            QStringLiteral("if then else elif fi case esac for while until do done in function return "
                                           "local export readonly set unset shift exit source declare"),
                            QStringLiteral("true false"));
    shell.sigils = true;
    table.insert(QStringLiteral("shell"), shell);

    auto cmake = hashFamily(QStringLiteral("CMake"),
                            QStringLiteral("if elseif else endif foreach endforeach function endfunction macro "
                                           "endmacro set option return"),
                            QStringLiteral("ON OFF TRUE FALSE"));
    cmake.sigils = true;
    table.insert(QStringLiteral("cmake"), cmake);

    LanguageRules json;
    json.name = QStringLiteral("JSON");
    json.constants = words(QStringLiteral("true false null"));
    json.quotes = QStringLiteral("\"");
    table.insert(QStringLiteral("json"), json);

    auto yaml = hashFamily(QStringLiteral("YAML"), {}, QStringLiteral("true false null yes no on off ~"));
    yaml.bareKeys = true;
    table.insert(QStringLiteral("yaml"), yaml);

    auto toml = hashFamily(QStringLiteral("TOML"), {}, QStringLiteral("true false"));
    toml.bareKeys = true;
    toml.sections = true;
    table.insert(QStringLiteral("toml"), toml);

    LanguageRules markdown;
    markdown.name = QStringLiteral("Markdown");
    markdown.markdown = true;
    table.insert(QStringLiteral("markdown"), markdown);
    return table;
}

QString languageKey(const QString &path)
{
    static const QHash<QString, QString> bySuffix = {
        { QStringLiteral("c"), QStringLiteral("cpp") },      { QStringLiteral("h"), QStringLiteral("cpp") },
        { QStringLiteral("cc"), QStringLiteral("cpp") },     { QStringLiteral("cpp"), QStringLiteral("cpp") },
        { QStringLiteral("cxx"), QStringLiteral("cpp") },    { QStringLiteral("hpp"), QStringLiteral("cpp") },
        { QStringLiteral("hh"), QStringLiteral("cpp") },     { QStringLiteral("hxx"), QStringLiteral("cpp") },
        { QStringLiteral("rs"), QStringLiteral("rust") },    { QStringLiteral("go"), QStringLiteral("go") },
        { QStringLiteral("py"), QStringLiteral("python") },  { QStringLiteral("pyi"), QStringLiteral("python") },
        { QStringLiteral("js"), QStringLiteral("js") },      { QStringLiteral("mjs"), QStringLiteral("js") },
        { QStringLiteral("cjs"), QStringLiteral("js") },     { QStringLiteral("jsx"), QStringLiteral("js") },
        { QStringLiteral("ts"), QStringLiteral("js") },      { QStringLiteral("tsx"), QStringLiteral("js") },
        { QStringLiteral("qml"), QStringLiteral("qml") },    { QStringLiteral("rb"), QStringLiteral("ruby") },
        { QStringLiteral("rake"), QStringLiteral("ruby") },  { QStringLiteral("gemspec"), QStringLiteral("ruby") },
        { QStringLiteral("sh"), QStringLiteral("shell") },   { QStringLiteral("bash"), QStringLiteral("shell") },
        { QStringLiteral("zsh"), QStringLiteral("shell") },  { QStringLiteral("json"), QStringLiteral("json") },
        { QStringLiteral("yml"), QStringLiteral("yaml") },   { QStringLiteral("yaml"), QStringLiteral("yaml") },
        { QStringLiteral("toml"), QStringLiteral("toml") },  { QStringLiteral("md"), QStringLiteral("markdown") },
        { QStringLiteral("markdown"), QStringLiteral("markdown") }, { QStringLiteral("cmake"), QStringLiteral("cmake") },
    };
    const QFileInfo info(path);
    const auto name = info.fileName();
    if (name == QStringLiteral("CMakeLists.txt"))
        return QStringLiteral("cmake");
    if (name == QStringLiteral("Gemfile") || name == QStringLiteral("Rakefile"))
        return QStringLiteral("ruby");
    if (name == QStringLiteral("PKGBUILD") || name.startsWith(QStringLiteral(".bash")))
        return QStringLiteral("shell");
    return bySuffix.value(info.suffix().toLower());
}

} // namespace

const LanguageRules *languageForPath(const QString &path)
{
    static const auto languages = buildLanguages();
    const auto found = languages.constFind(languageKey(path));
    return found == languages.cend() ? nullptr : &found.value();
}

} // namespace GitNaga
