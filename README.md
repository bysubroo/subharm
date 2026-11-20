# SubHarm

**Bekovi koji znaju pesmu.** SubHarm je VST3/AU plugin koji staviš na lead vokal. Kroz side chain čuje beat, sam pročita akorde, ključ i tempo i od tvog glasa napravi stack harmonija (3, 4 ili 8 glasova) koje prate promene akorda. Sve radi lokalno, bez kredita i bez interneta.

Ličan alat Studija SubRoo. Klon ideje Controlla Stacks, napisan od nule.

---

## Instalacija

Sve verzije su na GitHub **Releases** (stare ostaju dostupne).

- **Windows:** `SubHarm-x.y.z-Windows-Setup.exe` instalira VST3 plugin (`C:\Program Files\Common Files\VST3`) i aplikaciju **SubHarm** (Start meni). Deinstalacija: Settings → Apps → SubHarm.
- **macOS (Apple silicon + Intel):** `SubHarm-x.y.z-macOS.pkg` instalira VST3, AU i aplikaciju u Applications. Installer nije potpisan Apple sertifikatom: prvi put desni klik → *Open*.

Posle instalacije uradi rescan plugina u DAW-u.

**Standalone aplikacija:** isti SubHarm bez DAW-a. U Audio Settings izaberi ulaze: 1/2 je vokal, 3/4 je side chain (beat). RECORD odmah snima, a ponovni klik zaustavlja. Stackove izvlačiš preko MIX/STEMS ili Export-a. Reprodukcija stackova uz timeline postoji samo u DAW-u.

## Kako se koristi

1. **Ubaci SubHarm kao audio efekat na lead vokal.**
2. **Podesi side chain na beat**, ili na bus sa akordima (bolje: track sa držanim akordima).
   - **Cubase:** u headeru plugina uključi *Side-Chain*, pa na beat/chords tracku dodaj send ka *SubHarm – Side-Chain*.
   - **FL Studio:** na mixer tracku beata desni klik na strelicu ka vokal tracku → *Sidechain to this track*, pa u wrapperu SubHarma poveži sidechain ulaz.
   - Lampica **SIDE CHAIN** u headeru zasvetli zeleno kad zvuk stiže.
3. **RECORD THIS TRACK**, pa play u DAW-u. Stop kad završiš. Vokal i beat ulaze kao jedan take, bez bounce-a. Najbolje je da kreneš od početka takta.
4. **Nacrtaj delove (parts)** prevlačenjem po vokalu. Ivice se hvataju za šesnaestine (Alt = slobodno). Klik na akord ako ga je pogrešno pročitao.
5. **Key** (Auto ili ručno, može i pre snimanja), **Voices / Delivery**, pa izaberi **linije** (ili preset Short/Tall/Full), **Flavour / Humanize** → **STACK HARMONIES**. Dok jedan deo renderuje, slobodno crtaš sledeći.
6. **Stackovi sviraju zaključani za transport DAW-a**, i u offline bounce-u. Štikliraj koje hoćeš da čuješ.
7. **MIX** (jedan stereo track) ili **STEMS** (suvo, po liniji i strani: `SubHarm 3RD UP L`, `SubHarm 3RD UP R`, `SubHarm CHORD HARMONY HIGH L`...) prevuci direktno na timeline. Fajl počinje od početka take-a, pa ga spusti na takt gde si krenuo sa snimanjem. Cubase može i *Move to Origin*, jer fajl nosi BWF timestamp. **Export...** snima sve u folder.

Projekat (take, akordi, delovi, stackovi) se čuva sa DAW projektom. Audio je u `Documents/SubHarm/Sessions/<id>/`.

### Kontrole

| Kontrola | Šta radi |
|---|---|
| **Key** | Auto (prepoznaje sam) ili ručno izabran ključ. Ručni ključ zaključava i akorde: prepoznaju se samo akordi iz tog ključa. Promena ključa posle snimanja ponovo čita akorde |
| **Lines** | **UNIS**: jedna konstantna nota na tonici ključa (pedal, npr. F# u F# minoru) · **UNIS 5TH**: pedal na kvinti ključa (G u C-duru) · **DOUBLE**: tvoja melodija duplirana (Revoice-stil, široko L/R) · 3RD UP/DOWN · 5TH UP/DOWN · OCT UP/DOWN · CHORD HI/LO. Svaka linija se peva dvaput (L + R take). Intervalne linije idu striktno po skali ključa, a CHORD linije biraju tonove akorda (u ključu) sa voice leadingom |
| **Preset** | Short: UNIS + 3RD UP · **SubRoo (default)**: UNIS + 3RD UP + OCT UP + OCT DOWN · Full: DOUBLE + UNIS + UNIS 5TH + 3RD UP + 3RD DOWN + OCT UP + OCT DOWN + CHORD HI |
| **Flavour** | Boji CHORD linije. Clean: trozvuci. Pop: + add9. R&B: + septime i none. Gospel: + 11/13 |
| **Humanize** | Tight / Natural / Loose: svaki glas je svoj "take", kao Revoice Doubler: kašnjenje po noti (ranije ili kasnije), pomeraj visine po noti, scoop u notu, drift, dubina vibrata, varijacija glasnoće i formanta. Svaki glas ima i svoj šum/dah, pa nema faziranja |
| **Voices** | Your voice / Wide / Deeper / Lighter crew: formanti po glasu |
| **Delivery** | Natural / Airy (dah u visinama) / Gentle (mekše, manje vibrata) / Full voice (telo) / Energetic (prezens) |
| **Sauce** | Širina stereo slike + vazduh + malo saturacije (za playback i MIX; STEMS ostaju suvi) |
| **Level** | Glasnoća stackova u playbacku |
| **Hear lead** | Isključi da čuješ samo stackove |

## Kako radi (gde da menjaš)

```
src/core/            čist C++ bez JUCE-a, testira se posebno
  ChordAnalysis.cpp  semitonski spektar → skidanje overtonova → chroma po udarcu → Viterbi akordi, ključ
  Harmony.cpp        nota-segmentacija leada + "mozak": koji ton peva koja linija, voice leading, flavour pravila
  Renderer.cpp       WORLD vokoder: f0 pomeranje sa očuvanim formantima, humanize, delivery, crew, mix/sauce
src/plugin/          JUCE plugin: snimanje, timeline, UI, playback, export, čuvanje
tests/               core_test (sintetička pesma) + host_test (simulira DAW sesiju)
```

Najčešće izmene:
- **Pravila harmonije po flavouru:** `chordSet()` u `Harmony.cpp`
- **Gde sede linije** (terca gore/dole...): `kSlotTarget` / `kSlotDegrees` u `Harmony.cpp`
- **Zvuk izvedbe:** `deliveryFor()` i `deliveryDb()` u `Renderer.cpp`
- **Humanize vrednosti:** `humanizeFor()` u `Renderer.cpp`

## Build

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target SubHarm_VST3
```

JUCE 8.0.10 i WORLD se skidaju automatski. Svaki push na `main` pravi Windows i macOS build u GitHub Actions. Tag `v0.x.y` pravi GitHub Release sa zipovima.

## Plan

- **v1 (ovo):** DSP render tvog glasa. Odličan za guide pevačima i za guste stackove u miksu.
- **v2: SubHarm Engine.** Lokalni AI render (zero-shot singing voice conversion, npr. Seed-VC) na GPU-u, koji svaku liniju ponovo "otpeva" glasom pevača iz samog take-a. Plugin samo bira render mod.

Licence: JUCE (AGPLv3 / komercijalna), WORLD (modified BSD). Za ličnu upotrebu.
