﻿// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// text.h - english version

STRING(STR_TITLE, "Archive ZIP à auto-extraction")
STRING(STR_ERROR, "Erreur")
STRING(STR_QUESTION, "Question")
STRING(STR_ERROR_FOPEN, "Impossible d'ouvrir le fichier d'archive: ")
STRING(STR_ERROR_FSIZE, "Impossible d'obtenir la taille du fichier d'archive: ")
STRING(STR_ERROR_MAXSIZE, "Taille maximale de l'archive atteinte.")
STRING(STR_ERROR_FMAPPING, "Impossible de mapper l'archive en mémoire: ")
STRING(STR_ERROR_FVIEWING, "Impossible de créer la vue du mapping de l'archive en mémoire: ")
STRING(STR_ERROR_NOEXESIG, "Aucune signature EXE trouvée.")
STRING(STR_ERROR_NOPESIG, "Aucune signature d'exécutable PE trouvée.")
STRING(STR_ERROR_INCOMPLETE, "Erreur de format de donnée - fichier incomplet.")
STRING(STR_ERROR_SFXSIG, "Aucune signature d'extraction automatique trouvée.")
STRING(STR_ERROR_TEMPPATH, "Impossible d'obtenir un chemin de fichier temporaire: ")
STRING(STR_ERROR_TEMPNAME, "Impossible d'obtenir un nom de fichier temporaire: ")
STRING(STR_ERROR_DELFILE, "Impossible de supprimer le fichier temporaire: ")
STRING(STR_ERROR_MKDIR, "Impossible de créer le répertoire temporaire: ")
STRING(STR_ERROR_GETATTR, "Impossible d'obtenir les attributs du fichier: ")
STRING(STR_ERROR_GETCWD, "Impossible d'obtenir le répertoire de travail courant: ")
STRING(STR_ERROR_BADDATA, "Erreur dans les données compressées.")
STRING(STR_ERROR_TOOLONGNAME, "Le nom de fichier est trop long.")
STRING(STR_LOWMEM, "Erreur, mémoire insuffisante: ")
STRING(STR_NOTENOUGHTSPACE, "Il ne semble pas y avoir assez d'espace sur le lecteur cible.\nVoulez-vous continuer ?\n\nExpace requis: %s octets.\nEspace disponible: %s octets.\n%s")
STRING(STR_ERROR_TCREATE, "Impossible de créer un nouveau thread: ")
STRING(STR_ERROR_METHOD, "Impossible d'extraire le fichier. Méthode de compression non supportée.")
STRING(STR_ERROR_CRC, "Echec de vérification du CRC.")
STRING(STR_ERROR_ACCES, "Impossible d'accéder au fichier: ")
STRING(STR_ERROR_WRITE, "Impossible d'écrire le fichier: ")
STRING(STR_ERROR_BREAK, "Voulez-vous arrêter l'extraction ?")
STRING(STR_ERROR_DLG, "Impossible de créer la fenêtre de dialog: ")
STRING(STR_ERROR_FCREATE, "Impossible de créer ou d'ouvrir le fichier: ")
STRING(STR_ERROR_WAIT, "Impossible d'attendre le processus: ")
STRING(STR_ERROR_WAIT2, "\nAppuyez sur OK lorsque les fichiers temporaires peuvent être effacés en toute sécurité.")
STRING(STR_ERROR_WAITTHREAD, "Impossible d'attendre le thread: ")
STRING(STR_ERROR_DCREATE, "Impossible de créer un nouveau répertoire: ")
STRING(STR_ERROR_DCREATE2, "Impossible de créer un nouveau répertoire: ce nom est déjà utilisé par un fichier.")
STRING(STR_ERROR_FCREATE2, "Impossible de créer le fichier: ce nom est déjà utilisé par un répertoire.")
STRING(STR_TARGET_NOEXIST, "Le répertoire cible n'existe pas, voulez-vous le créer ?")
STRING(STR_OVERWRITE_RO, "Voulez-vous écraser le fichier avec un attribut LECTURESEULE, SYSTÈME ou CACHÉ ?")
STRING(STR_STOP, "Arrêter")
STRING(STR_STATNOTCOMPLETE, "Aucun fichier extrait.")
STRING(STR_SKIPPED, "Nombre de fichiers ignorés:\t%d\n")
STRING(STR_STATOK, "Extraction terminée avec succès.\n\nNombre de fichiers extraits:\t%d\n%sTaille totale des fichiers extraits:\t%s O  ")
STRING(STR_BROWSEDIRTITLE, "Sélectionnez un répertoire.")
STRING(STR_BROWSEDIRCOMMENT, "Sélectionnez le répertoire où extraire les fichiers.")
STRING(STR_OVERWRITEDLGTITLE, "Confirmer l'écrasement de fichier")
STRING(STR_TEMPDIR, "&Répertoire de stockage des fichiers temporaires:")
STRING(STR_EXTRDIR, "&Répertoire où extraire les fichiers:")
STRING(STR_TOOLONGCOMMAND, "Impossible d'exécuter la commande: le nom de fichier est trop long")
STRING(STR_ERROR_EXECSHELL, "Impossible d'exécuter la commande: ")
STRING(STR_ADVICE, "\nNote: vous pouvez lancer une auto-extraction avec le paramètre \'-s\' et sélectionner vous-même le répertoire d'extraction des fichiers.")
STRING(STR_BADDRIVE, "Le nom du lecteur ou partage réseau spécifié est invalide.")
STRING(STR_ABOUT1, "&A propos >>")
STRING(STR_ABOUT2, "<< &A propos")
STRING(STR_OK, "OK")
STRING(STR_CANCEL, "Annuler")
STRING(STR_YES, "Oui")
STRING(STR_NO, "Non")
STRING(STR_AGREE, "J'accepte")
STRING(STR_DISAGREE, "Je refuse")
#ifndef EXT_VER
STRING(STR_ERROR_ENCRYPT, "Impossible d'extraire le fichier. Le fichier est chiffré.")
STRING(STR_PARAMTERS, "Paramètres de ligne de commande:\n\n-s\tMode sans échec. Affiche le dialogue principal de l'extracteur automatique\n\tet permet le changement du répertoire de destination.")
#else  //EXT_VER
STRING(STR_ERROR_TEMPCREATE, "Impossible de créer le fichier temporaire: ")
STRING(STR_ERROR_GETMODULENAME, "Impossible d'obtenir le nom du module: ")
STRING(STR_ERROR_SPAWNPROCESS, "Impossible de créer un nouveau processus: ")
STRING(STR_INSERT_NEXT_DISK, "Veuillez indiquer le numéro du disque %d.\nAppuyez sur OK quand c'est prêt.")
STRING(STR_ERROR_GETWD, "Impossible de récupérer le répertoire Windows: ")
STRING(STR_ADVICE2, "\nNote: vous pouvez lancer une auto-extraction avec le paramètre \'-t\' et sélectionner vous-même le répertoire de fichiers temporaires.")
STRING(STR_PARAMTERS, "Paramètres de la ligne de commande:\n\n"
                      "-s\t\tMode sans échec. Affiche le dialogue principal de l'extracteur automatique\n"
                      "\t\tet permet le changement du répertoire de destination.\n"
                      "-t [chemin]\t\tEnregistre les fichiers temporaires dans le répertoire spécifié.\n"
                      "-a [archive]\tExtrait l'archive.\n"
                      "\t\test le dernier volume de l'archive.\n\n"
                      "Note: les longs chemins de fichiers doivent être écrits entre guillemets.")
STRING(STR_ERROR_READMAPPED, "Impossible de lire le fichier source.")
STRING(STR_PASSWORDDLGTITLE, "Mot de passe requis")
#endif //EXT_VER

//STRING(STR_NOTSTARTED, "")
