# Démarrage rapide (15 minutes)

Du déballage au premier démarrage. Les détails sont dans
[USER_GUIDE.md](USER_GUIDE.md) ; l'installation de la PWA est dans
[PWA_INSTALL.md](PWA_INSTALL.md).

> La version russe fait foi. En cas de différence, suivez le texte russe.
> Les libellés de l'interface sont en russe par défaut («Старт», «Стоп»,
> «Система → API-ключ») ; les termes français ici sont des traductions.

## 1. Ce qu'il vous faut

- Un appareil Automatika assemblé avec le firmware V1.0.1.
- Un téléphone ou un ordinateur portable avec Wi-Fi 2,4 GHz.
- Une prise 220 V avec terre et disjoncteur différentiel (RCD).
- De l'eau de refroidissement et du liquide à distiller (au moins 15 % du volume de la cucurbite).

## 2. Raccordement

1. Raccordez le refroidissement (entrée/sortie d'eau).
2. Remplissez la cucurbite de liquide, sans descendre sous le niveau minimum.
3. Fermez le couvercle et vérifiez que l'E-STOP **n'est pas** enfoncé.
4. Mettez sous tension. L'écran doit afficher l'écran d'accueil, puis l'écran principal.

> N'allumez pas le chauffage sans cucurbite remplie et sans circulation d'eau de refroidissement.

## 3. Connexion au Wi-Fi

Au premier démarrage (ou après une réinitialisation), l'appareil crée un point d'accès
ouvert **`Automatika-Setup`**.

1. Sur le téléphone, ouvrez le Wi-Fi et connectez-vous à `Automatika-Setup`.
2. Lancez l'application **ESP SoftAP Prov** (Espressif, Android/iPhone) ou
   `esp_prov.py` d'ESP-IDF.
3. Appareil : `Automatika-Setup`, transport `softap`.
4. Saisissez le SSID et le mot de passe de votre réseau **2,4 GHz**.
5. Attendez le message de succès. L'appareil redémarre le Wi-Fi et rejoint votre réseau.

Le point d'accès n'a pas de mot de passe (provisionnement en sécurité niveau 0). Ne laissez
pas l'appareil en mode configuration dans un lieu fréquenté plus longtemps que nécessaire.

## 4. Ouvrir l'interface web

1. Dans le navigateur, ouvrez `http://automatika.local`
   (si mDNS ne fonctionne pas, utilisez l'adresse IP de l'appareil dans la liste des clients de votre routeur).
2. Allez dans **Système → Clé API**.
3. Appuyez sur **« Générer »**. La clé est créée et enregistrée dans ce navigateur. C'est
   une opération unique : tant qu'aucune clé n'existe, l'appareil accepte l'amorçage ; ensuite, seule la clé actuelle est acceptée.
4. Revenez à **Travail → Tableau de bord**. Le statut doit passer à `Online`.

> Le firmware n'a pas de clé par défaut. Si quelqu'un a créé une clé avant vous,
> réinitialisez les paramètres (voir SERVICE_MANUAL.md) et recommencez.

## 5. Installer la PWA (facultatif)

1. Ouvrez `https://automatika.local`.
2. Acceptez l'avertissement du certificat auto-signé (ou installez `cert.pem` comme
   certificat de confiance — [PWA_INSTALL.md](PWA_INSTALL.md)).
3. Chrome (Android) : bouton **« Installer »** dans l'en-tête ou dans le menu →
   « Installer l'application ».
4. Safari (iPhone) : « Partager » → « Sur l'écran d'accueil ».

Après l'installation, l'application s'ouvre sans barre d'adresse, fonctionne hors ligne
et propose des raccourcis vers « Tableau de bord », « Journal » et « Paramètres ».

## 6. Premier lancement d'un processus

1. Vérifiez les relevés : la cucurbite et la colonne doivent être proches de la température ambiante,
   et le débit de refroidissement supérieur à 0,5 l/min.
2. Réglez la puissance manuellement (par exemple 30–40 %) et vérifiez que la température monte.
3. Pour le mode automatique, configurez un profil et le PID (voir
   [USER_GUIDE.md](USER_GUIDE.md), section « Profils et PID »).
4. Démarrage : appuyez sur **« Démarrer »**, puis choisissez la tuile du mode (rectification, distillation,
   brassage, PVK, NBK, distillation sous vide). Arrêt : **« Arrêter »**.
5. Suivez le journal : onglet **« Journal »** (nécessite la clé API).

## 7. En cas de problème

- Ne chauffe pas — [TROUBLESHOOTING.md](../service/TROUBLESHOOTING.md), §2.1.
- Pas de Wi-Fi — §2.5.
- La PWA ne s'installe pas — §2.6 et [PWA_INSTALL.md](PWA_INSTALL.md).
- Alarme / E-STOP — coupez immédiatement l'alimentation, puis consultez
  [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md).

## 8. Et ensuite ?

- [USER_GUIDE.md](USER_GUIDE.md) — utilisation quotidienne.
- [FAQ.md](FAQ.md) — questions fréquentes.
- [video_links.md](video_links.md) — thèmes des vidéos tutorielles (sans liens pour l'instant).
- [SERVICE_MANUAL.md](../service/SERVICE_MANUAL.md) — maintenance.
