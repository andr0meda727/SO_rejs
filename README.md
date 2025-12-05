# System obsługi załadunku i rejsów statku pasażerskiego

## Opis projektu

Projekt symuluje system portowy z wykorzystaniem programowania współbieżnego.
Statek obsługuje pasażerów wchodzących przez mostek o ograniczonej przepustowości oraz wykonuje ograniczoną liczbę rejsów w ciągu dnia.
W systemie działają trzy procesy:

* **Pasażer**
* **KapitanStatku**
* **KapitanPortu**

Celem projektu jest prawidłowa synchronizacja procesów oraz zapewnienie bezpieczeństwa podczas załadunku, rejsu i wyładunku.

---

## Parametry systemu

* **N** – maksymalna liczba pasażerów, którzy mogą jednorazowo znajdować się na statku
* **K** – maksymalna liczba pasażerów mogących jednocześnie przebywać na moście (K < N)
* **T1** – czas pomiędzy planowanymi odpłynięciami statku
* **T2** – czas trwania rejsu
* **R** – maksymalna liczba rejsów dziennie

---

## Zasady działania

### Pasażerowie

* Mogą wejść na mostek tylko, gdy obecna liczba pasażerów na moście < **K**.
* Mogą wejść na statek tylko, gdy liczba pasażerów na pokładzie < **N**.
* Mostek działa jednokierunkowo — nie można jednocześnie wchodzić i schodzić.
* Po przypłynięciu statku pasażerowie opuszczają pokład przed rozpoczęciem kolejnego załadunku.

---

### Kapitan Statku

Odpowiada za:

* kontrolę liczby pasażerów na statku,
* pilnowanie, by podczas odpływania mostek był pusty,
* rozpoczęcie rejsu po:

  * upływie czasu **T1**, lub
  * otrzymaniu sygnału **sygnał1** (polecenie natychmiastowego odpłynięcia),
* zakończenie pracy po wykonaniu **R** rejsów lub po otrzymaniu **sygnał2**.

**Sygnał2**:

* jeśli dotrze podczas załadunku — rejs nie startuje, pasażerowie opuszczają statek, system kończy pracę,
* jeśli dotrze podczas rejsu — rejs kończy się normalnie, kolejne nie są wykonywane.

---

### Kapitan Portu

Może wysyłać dwa sygnały:

* **sygnał1** — natychmiastowy start rejsu (o ile mostek pusty)
* **sygnał2** — zakończenie pracy statku

Sygnały mogą pojawić się w dowolnym momencie działania systemu.

---

## Zaimplementowane procedury

### `Pasażer`

* Wejście na mostek (jeśli < K)
* Wejście na statek (jeśli < N)
* Oczekiwanie na koniec rejsu
* Wyjście ze statku i z mostka

---

### `KapitanStatku`

* Zliczanie pasażerów wewnątrz statku
* Pilnowanie pustego mostka przy odpływie
* Start rejsu (czasowy lub po sygnale)
* Obsługa sygnałów **sygnał1** i **sygnał2**
* Zatrzymanie systemu po R rejsach lub sygnał2

---

### `KapitanPortu`

* Generowanie sygnału **sygnał1**
* Generowanie sygnału **sygnał2**

---

## Cel projektu

* Zaprezentowanie problemu synchronizacji procesów
* Obsługa współdzielenia zasobów (mostek, statek)
* Zaprojektowanie poprawnego modelu komunikacji między procesami
* Zapobieganie sytuacjom niebezpiecznym, takim jak:

  * przekroczenie pojemności,
  * odpłynięcie z pasażerami na moście,
  * konflikt wejścia i wyjścia na mostku.
