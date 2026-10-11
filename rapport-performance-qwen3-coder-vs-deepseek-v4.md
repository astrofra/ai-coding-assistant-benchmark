# Rapport sommaire — Qwen3-Coder vs DeepSeek v4

**Objet :** comparaison des performances des deux modules testés dans ce dépôt
(`qwen3-coder-30b-a3b-instruct` et `deepseek-v4-flash-0731`) sur la tâche de
benchmark définie dans `documentation/raytracer-spec.md`.

**Méthode :** lecture du cahier des charges puis **inspection visuelle des images
produites** (`out.png` convertis depuis `out.tga` par `convert_tga_to_png.py`) et
vérification des en-têtes TGA. Aucune recompilation n'a été effectuée : ce rapport
porte sur la **qualité de résultat** (conformité au cahier des charges), et non sur
un temps d'exécution mesuré.

---

## 1. Rappel de la tâche

Le cahier des charges demande un raytracer **mono-fichier `main.c` en C99 strict**
rendant, via le compilateur local, une image **1280×720 `out.tga`** contenant :

- une **sphère de verre** à gauche (indice 1.5, réfraction + Fresnel, réfraction totale interne) ;
- une **sphère de chrome** à droite (réflexion quasi spéculaire, sans diffusion) ;
- un **damier infini** noir/blanc au sol, avec ombres portées et réflexions ;
- **trois point lights** (key/fill/rim), ombres dures, lumière transmise par le verre ;
- un **ciel dégradé** rouge→orange évalué depuis la direction du rayon ;
- un **brouillard** exponentiel mélangeant le sol au ciel ;
- ≥ 8 rebonds secondaires, ≥ 4 échantillons par pixel, gamma 2.2, en-tête TGA type 2
  descripteur `0x20` (origine haut-gauche).

---

## 2. Résultats — DeepSeek v4

Échantillon : 6 exécutions, `prompt-en-c99/codex-albert-deepseek4-coder-32768tokens-001..006`.

| Run | Orientation | Composition | Verre | Chrome | Ombres | Défauts notables |
| --- | --- | --- | --- | --- | --- | --- |
| 001 | ✅ correcte | ✅ | partiel (verre teinté orangé, anneaux internes) | partiel (haut orangé / bas damier) | ✅ | verre non incolore |
| 002 | ✅ correcte | ✅ | faible (teinté) | faible | ✗ peu visibles | aspect « plastique » |
| 003 | ✅ correcte | ✅ | ✗ opaque/diffus, très bruité | partiel | ✅ | sphère type « cuivre dépoli » |
| 004 | ✅ correcte | ✅ | ✗ **sphère entièrement blanche** (saturée) | partiel | ✅ | verre cassé |
| 005 | ❌ caméra sous le sol | ✗ | — | — | — | image = bandes, scène absente |
| 006 | ❌ vue inversée/sous le sol | ✗ | partiel | partiel | — | composition renversée |

- **Orientation et composition correctes : 4/6.** Les deux échecs (005, 006) sont
  des bugs franches de caméra/plan (vue depuis sous le sol).
- **Aucune image ne respecte réellement les matériaux** : le « verre » apparaît le
  plus souvent comme une sphère teintée (voire opaque ou blanche), le chrome est
  rendu de façon partielle (hémi-sphère réfléchissante).
- **Aucun artefact de canal de couleur** (pas de cyan/bleu parasite), **pas d'image
  verticalement inversée** sur les runs réussis.
- À noter : les logs `004`/`005` contiennent une `ContextWindowExceededError`
  (32768 tokens demandés pour un prompt de ~98k sur une fenêtre de 131072) — incident
  d'infrastructure/routeur, distinct de la qualité du modèle.

---

## 3. Résultats — Qwen3-Coder

Qwen3-Coder a été testé sur beaucoup plus de configurations : 8192 / 32768 / 65536
tokens, variantes « params optimized », prompts EN et FR, plus la version `prompt-en-c99`.
Extraits inspectés (échantillon d'environ 18 images) :

| Configuration inspectée | Orientation | Défauts principaux |
| --- | --- | --- |
| c99 32768tokens-001 | ✅ | damier parsemé de **cyan/bleu**, verre bruité |
| c99 32768tokens-002 | ❌ inversée | cyan, ciel en bas |
| c99 32768tokens-003 | ❌ inversée | grandes ombres, sol = plafond |
| c99 32768tokens-004 | ❌ inversée | sphère gauche entièrement blanche |
| c99 32768tokens-005 | ❌ | **scène vide** (ciel seul) |
| c99 65536tokens-001 | ❌ inversée | verre blanc |
| c99 65536tokens-002 | ❌ | verre = **bruit chromatique arc-en-ciel**, chrome = dégradé plat |
| c99 65536tokens-003 | ❌ inversée | bruit arc-en-ciel, sphère droite rose plate |
| c99 65536tokens-004 | ✅ | verre avec anneaux/moiré |
| c99 65536tokens-params-opt-001 | ❌ inversée | image **monochrome bleue** (bug de canal) |
| c99 65536tokens-params-opt-002 | ✅ | verre opaque orangé, chrome partiel, cyan au sol |
| c99 32768tokens-params-opt-001 | ❌ inversée | bruit arc-en-ciel |
| c99 32768tokens-params-opt-002 | ❌ inversée | moitié noire + bruit, sphère droite rouge plate |
| c99 32768tokens-params-opt-003 | ❌ inversée | cyan, sol = plafond |
| c99 65536tokens-params-opt-003 | ❌ inversée | verre blanc + taches cyan |
| c99 65536tokens-params-opt-004 | ❌ inversée | bruit arc-en-ciel |
| en 65536tokens-params-opt-001 | ✅ | verre avec anneaux, globalement correct |
| fr 32768tokens-005 | ✅ | **bruit arc-en-ciel** dans le verre |

- **Défaut structurel dominant : l'image est rendue verticalement inversée** (le sol
  apparaît comme un plafond, le ciel en bas) dans la majorité des runs inspectés.
  L'en-tête TGA déclare pourtant bien un descripteur `0x20` (origine haut-gauche) :
  le fichier est donc cohérent en métadonnées, mais les lignes/caméra sont inversées.
- **Artefacts de couleur cyan/bleu** sur le damier dans de nombreux runs (canaux
  hors bornes / valeurs non finies mal converties).
- **Sphère de verre très souvent cassée** : bruit chromatique arc-en-ciel (récursion
  non maîtrisée), rendu opaque, blanc saturé ou noir.
- Quelques runs atteignent néanmoins une orientation et une composition correctes
  (ex. `c99 65536tokens-004`, `prompt-en ...params-optimized-001`) avec un rendu de
  verre imparfait mais plausible.

---

## 4. Synthèse comparative

| Critère | DeepSeek v4 | Qwen3-Coder |
| --- | --- | --- |
| Orientation / caméra correcte | 4/6 | ~5/18 inspectés |
| Composition (2 sphères + damier) | 4/6 | ~7/18 inspectés |
| Damier sans artefact de couleur | oui (majorité des runs) | **non** (cyan/bleu fréquents) |
| Verre réellement incolore/réfractant | ✗ (teinté/opaque) | ✗ (bruité/opaque) |
| Chrome crédible | partiel | partiel |
| Échecs francs (image inexploitable) | 2/6 | nombreux (inversion, scène vide, monochrome) |
| Fiabilité globale | **plus stable** | **plus variable / fragile** |

### Différences de performance observées

1. **Respect des contraintes concrètes du cahier des charges.** DeepSeek v4 place
   correctement la caméra et la composition dans la plupart de ses runs ; Qwen3-Coder
   produit systématiquement (ou presque) une scène **inversée** ou vide.
2. **Robustesse numérique.** Qwen3-Coder génère régulièrement des **artefacts de
   couleur** (cyan/bleu, image monochrome) que DeepSeek v4 n'a pas produits sur
   l'échantillon.
3. **Rendu des matériaux.** Les deux modèles échouent sur le cœur physique de la tâche :
   ni l'un ni l'autre ne produit un verre incolore réfractant et un chrome vraiment
   spéculaire. Le verre de Qwen3-Coder est plus souvent « explosé » (bruit arc-en-ciel),
   celui de DeepSeek v4 plus souvent « faux mais stable » (teinté/opaque).
4. **Taux de réussite.** Sur l'échantillon, DeepSeek v4 délivre une image exploitable
   et conforme à la composition dans **4 runs sur 6** ; Qwen3-Coder n'y parvient que
   dans une minorité de runs, malgré un nombre d'essais beaucoup plus élevé et
   plusieurs réglages de paramètres d'échantillonnage.

---

## 5. Conclusion

Sur ce benchmark, **DeepSeek v4 se montre nettement plus fiable que Qwen3-Coder** :
il respecte plus souvent l'orientation, la composition et le format, et ne produit
pas les artefacts de canal de couleur qui affectent Qwen3-Coder. Qwen3-Coder reste
cependant capable de sorties correctes sur certaines configurations (notamment 65536
tokens), mais avec une variance élevée et des défauts structurels récurrents.

Aucun des deux modules ne satisfait pleinement l'exigence de **matériaux
physiquement corrects** (verre incolore + chrome spéculaire) : c'est le point faible
commun du benchmark.

---

### Limites de ce rapport

- L'inspection visuelle est **manuelle et non exhaustive** ; les décomptes Qwen3-Coder
  portent sur un échantillon (~18 images) et non sur la totalité des sorties.
- L'échantillon DeepSeek v4 est limité au répertoire `prompt-en-c99`, 32768 tokens
  (6 runs), alors que Qwen3-Coder couvre plusieurs tailles de contexte et réglages :
  la comparaison est **directionnelle**, pas un protocole contrôlé.
- Aucune mesure de temps de compilation ou d'exécution n'a été réalisée (consigne).
- Certains logs d'exécution (`ContextWindowExceededError` côté DeepSeek, erreurs
  JSON/routeur côté Qwen3-Coder) relèvent de l'infrastructure et non de la qualité
  des modèles.
