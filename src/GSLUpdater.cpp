#include "GSLUpdater.hpp"
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/hash.hpp>

using namespace geode::prelude;

// Откуда брать релизы: отдельный публичный репозиторий только с готовыми файлами
// (код мода остаётся в приватном).
static constexpr auto RELEASES_API = "https://api.github.com/repos/Chevo4ek/GSL-Integration-Releases/releases/latest";

static async::TaskHolder<web::WebResponse> s_check, s_sum, s_file;
static bool s_busy = false;

static web::WebRequest request() {
    auto req = web::WebRequest();
    req.userAgent("GSL-Integration-Updater");
    return req;
}

static void fail(std::string const& why) {
    s_busy = false;
    log::warn("GSL update: {}", why);
    FLAlertLayer::create("Update failed", why, "OK")->show();
}

static void install(std::string version, std::string fileUrl, std::string sumUrl) {
    s_busy = true;
    auto note = Notification::create("Downloading update...", NotificationIcon::Loading, 0.f);
    note->show();
    Ref<Notification> noteRef = note;

    // 1. контрольная сумма, 2. сам файл, 3. сверяем и заменяем
    s_sum.spawn(request().get(sumUrl), [=](web::WebResponse res) {
        if (!res.ok()) { noteRef->hide(); return fail("Couldn't download the checksum."); }
        auto sumText = res.string().unwrapOr("");
        if (sumText.size() < 64) { noteRef->hide(); return fail("The checksum file is broken."); }
        auto expected = sumText.substr(0, 64);
        for (auto& c : expected) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        s_file.spawn(request().get(fileUrl), [=](web::WebResponse res) {
            noteRef->hide();
            if (!res.ok()) return fail("Couldn't download the update.");
            auto const& data = res.data();
            auto actual = sha256(std::span<uint8_t const>(data.data(), data.size())).toString();
            if (actual != expected) return fail("The downloaded file doesn't match its checksum, nothing was changed.");

            // защита: внутри файла должна быть именно новая версия (не перепутан ли файл в релизе)
            auto tmp = Mod::get()->getPackagePath();
            tmp += ".download";
            if (auto w = file::writeBinary(tmp, data); !w) return fail("Couldn't save the update.");
            auto meta = ModMetadata::createFromGeodeFile(tmp);
            std::error_code ec;
            if (meta.hasErrors() || meta.getID() != Mod::get()->getID()
                || !(meta.getVersion() > Mod::get()->getVersion())) {
                std::filesystem::remove(tmp, ec);
                return fail("The release file is not a newer version of the mod. Nothing was changed.");
            }
            std::filesystem::remove(tmp, ec);

            auto path = Mod::get()->getPackagePath();
            if (auto w = file::writeBinarySafe(path, data); !w) {
                return fail(fmt::format("Couldn't save the update: {}", w.unwrapErr()));
            }
            s_busy = false;
            createQuickPopup(
                "Updated",
                fmt::format("<cg>Global Shitty List Integration {}</c> is installed.\nRestart the game to use it.", version),
                "Later", "Restart",
                [](FLAlertLayer*, bool restart) { if (restart) game::restart(true); }
            );
        });
    });
}

void gslCheckForUpdate(bool manual) {
    if (s_busy) return;
    s_check.spawn(request().header("Accept", "application/vnd.github+json").get(RELEASES_API), [manual](web::WebResponse res) {
        if (!res.ok()) {
            log::warn("GSL update check failed ({})", res.code());
            if (manual) FLAlertLayer::create("Update", "Couldn't check for updates.", "OK")->show();
            return;
        }
        auto json = res.json().unwrapOr(matjson::Value());
        auto tag = json["tag_name"].asString().unwrapOr("");
        auto latest = VersionInfo::parse(tag);
        if (!latest) return;
        if (!(latest.unwrap() > Mod::get()->getVersion())) {
            if (manual) FLAlertLayer::create("Update", "You have the latest version.", "OK")->show();
            return;
        }

        std::string fileUrl, sumUrl;
        if (auto assets = json["assets"].asArray()) {
            for (auto& a : assets.unwrap()) {
                auto name = a["name"].asString().unwrapOr("");
                auto url = a["browser_download_url"].asString().unwrapOr("");
                if (name.ends_with(".geode")) fileUrl = url;
                else if (name.ends_with(".geode.sha256")) sumUrl = url;
            }
        }
        if (fileUrl.empty() || sumUrl.empty()) return;

        auto version = latest.unwrap().toVString();
        createQuickPopup(
            "Update available",
            fmt::format("Version <cy>{}</c> of Global Shitty List Integration is out.\nYou have <cr>{}</c>. Update now?",
                version, Mod::get()->getVersion().toVString()),
            "Later", "Update",
            [version, fileUrl, sumUrl](FLAlertLayer*, bool update) {
                if (update) install(version, fileUrl, sumUrl);
            }
        );
    });
}
