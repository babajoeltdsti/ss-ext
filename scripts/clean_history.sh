#!/usr/bin/env bash
# Repo geçmişinden hassas bilgileri temizlemek için yardımcı script
# Kullanıcı: Bu scripti yerel olarak çalıştırın. Force-push gerekecektir.

set -euo pipefail

if ! command -v git >/dev/null 2>&1; then
  echo "git bulunamadi. Lütfen git kurulu olduğundan emin olun."
  exit 1
fi

if command -v bfg >/dev/null 2>&1; then
  echo "BFG bulundu. BFG ile temizleme yapılacak."
  read -p "Remote repo URL (örnek: https://github.com/kullanici/repo.git): " REPO_URL
  git clone --mirror "$REPO_URL" repo-mirror.git
  cd repo-mirror.git
  # Örnek: config.py içindeki e-posta parolalarını sil (tüm dosyayı veya kelimeyi hedefleyin)
  # Burada 'SSEXT_EMAIL_PASSWORD' gibi anahtar kelimeyi kaldırıyoruz
  echo 'SSEXT_EMAIL_PASSWORD' > passwords-to-delete.txt
  bfg --delete-files passwords-to-delete.txt
  bfg --delete-text passwords-to-delete.txt || true
  git reflog expire --expire=now --all
  git gc --prune=now --aggressive
  echo "Force push ediliyor..."
  git push --force
  cd ..
  echo "Temizleme tamamlandi. Diger kullanicilar repo'yu yeniden klonlamali."
  exit 0
fi

if command -v git-filter-repo >/dev/null 2>&1; then
  echo "git-filter-repo bulundu. filter-repo ile temizleme yapılacak."
  read -p "Remote repo URL (örnek: https://github.com/kullanici/repo.git): " REPO_URL
  git clone --mirror "$REPO_URL" repo-mirror.git
  cd repo-mirror.git
  # Örnek: belirli bir dosyayı geçmişten tamamen kaldırmak için:
  # git-filter-repo --invert-paths --path config.py
  echo "Lütfen hangi dosya veya metinleri temizlemek istediğinizi belirtin."
  echo "Örnegin: --replace-text replacements.txt (replacements.txt içindeki patternler)"
  echo "Ayrintili dokumantasyon: https://github.com/newren/git-filter-repo"
  exit 0
fi

# Eğer ne BFG ne de git-filter-repo yoksa, kullanıcıya adımları göster
cat <<'EOF'
BFG veya git-filter-repo yüklü değil. Aşağıdaki adımları manuel olarak uygulayabilirsiniz:

1) BFG kullanıyorsanız:
   a) git clone --mirror https://github.com/kullanici/repo.git
   b) echo 'SSEXT_EMAIL_PASSWORD' > passwords-to-delete.txt
   c) bfg --delete-text passwords-to-delete.txt repo.git
   d) cd repo.git
   e) git reflog expire --expire=now --all
   f) git gc --prune=now --aggressive
   g) git push --force

2) git-filter-repo kullanmak isterseniz dokümantasyona bakin:
   https://github.com/newren/git-filter-repo

Not: Force-push işlemi repo geçmişini değiştirir; ekip üyelerini bilgilendirin.
EOF
