# Démarrage rapide (15 minutes)

Du carton au premier processus. Guide complet : [USER_GUIDE.md](USER_GUIDE.md) ;
installation PWA : [PWA_INSTALL.md](PWA_INSTALL.md).

## 1. Matériel nécessaire

- Automatika avec firmware V0.9 beta.
- Téléphone ou ordinateur en Wi-Fi 2,4 GHz.
- Prise 220 V avec différentiel et mise à la terre.
- Eau de refroidissement et liquide à distiller (au moins 15 % du volume).

## 2. Branchement

1. Raccordez le refroidissement (entrée et sortie d'eau).
2. Remplissez le cube au-dessus du niveau minimal.
3. Fermez le couvercle et vérifiez que **l'arrêt d'urgence n'est pas enfoncé**.
4. Mettez sous tension : l'écran principal doit apparaître.

> Ne chauffez jamais un cube vide ou sans refroidissement.

## 3. Wi-Fi

Au premier démarrage l'appareil crée le réseau **`Automatika-Setup`**.

1. Connectez-vous à ce réseau depuis le téléphone.
2. Ouvrez **ESP SoftAP Prov** ou `esp_prov.py` d'ESP-IDF.
3. Saisissez le SSID et le mot de passe du réseau 2,4 GHz.
4. Attendez la confirmation et la reconnexion.

Le réseau de configuration est ouvert : ne le laissez pas actif inutilement
dans un lieu public.

## 4. Interface web

1. Ouvrez `http://automatika.local` (ou l'adresse IP du routeur).
2. Allez dans **Système → Clé API** puis **Générer**.
3. La clé est stockée dans ce navigateur ; sans elle seul le premier
   enregistrement est autorisé.
4. Revenez à **Travail → Tableau de bord** : l'état doit être `Online`.

## 5. PWA (facultatif)

1. Ouvrez `https://automatika.local` et acceptez le certificat auto-signé.
2. Chrome (Android) : **Installer** dans l'en-tête.
3. Safari (iPhone) : **Partager → Sur l'écran d'accueil**.

## 6. Premier processus

1. Vérifiez que le cube et la colonne sont à température ambiante et que le
   débit de refroidissement dépasse 0,5 l/min.
2. Réglez la puissance manuelle (30–40 %) et vérifiez que la température monte.
3. Pour le mode automatique, configurez le profil et le PID.
4. **Arrêter** coupe le chauffage ; **Safety** réarme seulement si le processus
   est arrêté, l'arrêt d'urgence relâché et le niveau correct.

> Sécurité : l'arrêt d'urgence, le relais NF de sécurité et la vérification
> active du boucle complètent l'installation électrique, ils ne la remplacent pas.
