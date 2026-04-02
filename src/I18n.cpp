#include "I18n.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace ssext {

namespace {

using Dict = std::unordered_map<std::string, std::string>;

const Dict kTr = {
    {"volume_title", "Ses Seviyesi"},
  {"update_title", "Güncelleme"},
  {"update_latest", "Güncel sürüm kullanıyorsun"},
  {"update_new", "Yeni sürüm: "},
  {"update_downloading", "İndiriliyor..."},
  {"update_installing", "Kurulum başlatılıyor..."},
  {"update_check_failed", "Güncelleme kontrolü başarısız."},
  {"update_apply_failed", "Güncelleme kurulamadı."},
  {"update_apply_started", "Güncelleme kuruluyor. Uygulama yeniden başlatılıyor..."},
  {"temp_prefix", "Sıcaklık: "},
  {"temp_na", "Sıcaklık: N/A"},
    {"notification_generic", "Yeni bildirim"},
  {"version_label", "Sürüm"},
  {"shutdown", "Kapatılıyor"},
  {"game_played", "Oynama "},
  {"language_changed_title", "Yeni Dil"},
  {"language_name_tr", "Türkçe"},
  {"language_name_en", "English"},
  {"language_name_de", "Deutsch"},
  {"language_name_fr", "Français"},
  {"language_name_es", "Español"},
};

const Dict kEn = {
    {"volume_title", "Volume"},
    {"update_title", "Update"},
    {"update_latest", "You are up to date"},
    {"update_new", "New version: "},
    {"update_downloading", "Downloading..."},
    {"update_installing", "Starting install..."},
  {"update_check_failed", "Update check failed."},
  {"update_apply_failed", "Update could not be installed."},
  {"update_apply_started", "Installing update. Restarting app..."},
    {"temp_prefix", "Temp: "},
    {"temp_na", "Temp: N/A"},
    {"notification_generic", "Notification"},
  {"version_label", "Version"},
  {"shutdown", "Shutting down"},
  {"game_played", "Played "},
  {"language_changed_title", "New Language"},
  {"language_name_tr", "Turkish"},
  {"language_name_en", "English"},
  {"language_name_de", "German"},
  {"language_name_fr", "French"},
  {"language_name_es", "Spanish"},
};

const Dict kDe = {
  {"volume_title", "Lautstärke"},
  {"update_title", "Update"},
  {"update_latest", "Du bist auf dem neuesten Stand"},
  {"update_new", "Neue Version: "},
  {"update_downloading", "Wird heruntergeladen..."},
  {"update_installing", "Installation wird gestartet..."},
  {"update_check_failed", "Update-Prüfung fehlgeschlagen."},
  {"update_apply_failed", "Update konnte nicht installiert werden."},
  {"update_apply_started", "Update wird installiert. App wird neu gestartet..."},
  {"temp_prefix", "Temp: "},
  {"temp_na", "Temp: N/V"},
  {"notification_generic", "Neue Benachrichtigung"},
  {"version_label", "Version"},
  {"shutdown", "Wird beendet"},
  {"game_played", "Gespielt "},
  {"language_changed_title", "Neue Sprache"},
  {"language_name_tr", "Türkçe"},
  {"language_name_en", "English"},
  {"language_name_de", "Deutsch"},
  {"language_name_fr", "Français"},
  {"language_name_es", "Español"},
};

const Dict kFr = {
  {"volume_title", "Volume"},
  {"update_title", "Mise à jour"},
  {"update_latest", "Vous êtes déjà à jour"},
  {"update_new", "Nouvelle version : "},
  {"update_downloading", "Téléchargement..."},
  {"update_installing", "Installation en cours..."},
  {"update_check_failed", "Échec de la vérification des mises à jour."},
  {"update_apply_failed", "La mise à jour n'a pas pu être installée."},
  {"update_apply_started", "Mise à jour en cours. Redémarrage de l'application..."},
  {"temp_prefix", "Temp : "},
  {"temp_na", "Temp : N/D"},
  {"notification_generic", "Nouvelle notification"},
  {"version_label", "Version"},
  {"shutdown", "Arrêt en cours"},
  {"game_played", "Joué "},
  {"language_changed_title", "Nouvelle langue"},
  {"language_name_tr", "Türkçe"},
  {"language_name_en", "English"},
  {"language_name_de", "Deutsch"},
  {"language_name_fr", "Français"},
  {"language_name_es", "Español"},
};

const Dict kEs = {
  {"volume_title", "Volumen"},
  {"update_title", "Actualización"},
  {"update_latest", "Ya estás actualizado"},
  {"update_new", "Nueva versión: "},
  {"update_downloading", "Descargando..."},
  {"update_installing", "Iniciando instalación..."},
  {"update_check_failed", "La comprobación de actualizaciones falló."},
  {"update_apply_failed", "No se pudo instalar la actualización."},
  {"update_apply_started", "Actualización en curso. Reiniciando la aplicación..."},
  {"temp_prefix", "Temp: "},
  {"temp_na", "Temp: N/D"},
  {"notification_generic", "Nueva notificación"},
  {"version_label", "Versión"},
  {"shutdown", "Cerrando"},
  {"game_played", "Jugado "},
  {"language_changed_title", "Nuevo idioma"},
  {"language_name_tr", "Türkçe"},
  {"language_name_en", "English"},
  {"language_name_de", "Deutsch"},
  {"language_name_fr", "Français"},
  {"language_name_es", "Español"},
};

const std::string kEmpty;

}  // namespace

void I18n::SetLanguage(const std::string& language_code) {
  std::string lowered = language_code;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  if (lowered == "en" || lowered == "tr" || lowered == "de" || lowered == "fr" ||
      lowered == "es") {
    language_ = lowered;
  }
}

const std::string& I18n::GetLanguage() const {
  return language_;
}

std::string I18n::Text(const std::string& key) const {
  const Dict* dict = &kTr;
  if (language_ == "en") {
    dict = &kEn;
  } else if (language_ == "de") {
    dict = &kDe;
  } else if (language_ == "fr") {
    dict = &kFr;
  } else if (language_ == "es") {
    dict = &kEs;
  }

  auto it = dict->find(key);
  if (it != dict->end()) {
    return it->second;
  }

  auto tr_it = kTr.find(key);
  if (tr_it != kTr.end()) {
    return tr_it->second;
  }

  return kEmpty;
}

}  // namespace ssext
