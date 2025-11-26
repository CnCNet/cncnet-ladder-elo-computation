# CnCNet Ladder ELO Computation

Computes ladder ELO ratings from completed games.

This ELO generator uses [Glicko-2](https://glicko.net/glicko/glicko2.pdf), a successor of the classical ELO algorithm.

## How does it work and what do these values actually mean?

Glicko-2 is a rating system based on statistical prediction. The difference between two players' ratings provides an estimate of the expected win probability when they face each other. There's an [online win rate calculator](https://sandhoefner.github.io/chess.html) to get the exact numbers.

If you look at two players who have played many games against each other, their actual win rate tends to align closely with the expected win probability based on their ratings. Naturally, individual results will vary — that's why Glicko-2 includes a measure of uncertainty called the rating **deviation**.

Glicko-2 adjusts ratings based not only on wins and losses but also on how consistent a player’s performance is over time. Players with fewer games or inconsistent results will have higher rating deviation, reflecting greater uncertainty. Over time, as more games are played, the system refines both the rating and the confidence in that rating.

The system requires you to constantly prove that you are as good as your rating (ELO) suggests. If you stop playing, your rating deviation increases over time. Once it rises above a certain threshold (around 100), you're no longer considered an active player.

You become active again once your deviation drops below approximately 65. As a new player, you typically need around 20–30 games for your deviation to fall below that threshold and receive your first stable ELO rating.

Glicko-2 is also well-suited for tracking your skill level over long periods of time.
Your rating from a year ago can be meaningfully compared to your current rating, giving you a clear indication of whether your performance has improved or declined.

## Changes to Glicko-2

The ELO generator uses the original Glicko-2 algorithm along with the suggested starting values, but some changes had to be made for handling new players.

To establish a rating quickly, Glicko-2 expects that new players are initially matched with opponents of roughly equal strength. However, when a new player starts at **1500/350** and loses a game against an opponent with **1500 ELO** and low deviation, their rating might drop to around **1300/320**. The system will then try to pair them with players around **1300 ELO**. If the new player continues to lose, they keep getting matched with lower-rated opponents.

Due to the limited player base on **CnCNet**, it's not possible to provide suitable opponents. As a result, new players often face a losing streak when joining the network, which distorts their initial rating, sometimes even reaching negative values. If these players eventually start winning games, they can deal disproportionate blows to the ratings of established players.

### Adjustments Made:

- Use **single-step mode** instead of batch mode for each game played if a player has **only wins or only losses**.
- As soon as a player has **both wins and losses**, recompute their rating:
  - Try various starting ELOs ranging from **100 to 3000**.
  - For each starting ELO, simulate submitting all played games in **batch mode**.
  - Select the starting ELO that results in the **smallest overall change** after processing all games.
  - This value is considered the player's **true performance** and becomes their **initial ELO**.
  - From that point on, continue using the **standard Glicko-2 algorithm**.


## Win expectations based on rating difference

Here's a table with win rate expectations:

| ELO difference | Expected win rate |
|----------------|-------------------|
|           +500 |            94.68% |
|           +450 |            93.02% |
|           +400 |            90.91% |
|           +350 |            88.23% |
|           +300 |            84.90% |
|           +250 |            80.83% |
|           +200 |            75.97% |
|           +150 |            70.34% |
|           +100 |            64.01% |
|           + 50 |            57.15% |
|           +  0 |            50.00% |
|           - 50 |            42.85% |
|           -100 |            35.99% |
|           -150 |            29.66% |
|           -200 |            24.03% |
|           -250 |            19.17% |
|           -300 |            15.10% |
|           -350 |            11.77% |
|           -400 |             9.09% |
|           -450 |             6.98% |
|           -500 |             5.32% |

If your ELO is 100 points below your opponent’s, you’re expected to win about 36% of the games.
If you win more than that, you'll gain ELO; if you win less, you'll lose ELO.


## ELO for team games

ELO is not originally designed for team games. There are algorithms specifically made for this purpose, like [TrueSkill](https://en.wikipedia.org/wiki/TrueSkill), but using those adds a lot of complexity. That's why we stick with ELO (Glicko-2) and make some adjustments to make it work for 2v2 games.

The simplest approach is to split up the game into two individual matches. If your team wins, it's treated as two wins - one against each of your opponents. Obviously, this leads to poor results because your ELO heavily depends on which teammates you get. Here's a result when this idea is applied.

| Difference team ELO | Expected | Actual |
|---------------------|----------|--------|
| +0                  | 50.00    |  50.58 |
| +50                 | 57.15    |  57.71 |
| +100                | 64.01    |  65.62 |
| +150                | 70.34    |  72.80 |
| +200                | 75.97    |  77.65 |
| +250                | 80.83    |  82.59 |
| +300                | 84.90    |  88.25 |
| +350                | 88.23    |  90.66 |
| +400                | 90.91    |  93.72 |
| +450                | 93.02    |  95.81 |
| +500                | 94.68    |  95.96 |
| **Average Error**   |          |   1.94 |

That’s a large margin of error. When the team ELO (i.e., the sum of both players' ratings) differs significantly, the predictions become inaccurate. The stronger team tends to win much more often than expected. While this system might be good enough for ranking players, the actual ratings don't make sense. If you form teams with the same team ELO, matches are balanced, but as soon as ELOs diverge, the results deviate from expectations.

A better approach is to assign a share of the win or loss based on the percentage of ELO you contribute to your team's total ELO. Based on this share, an imaginary opponent is calculated from the opponent team's ELO. Here's an example:

 * Your ELO: 1850
 * Your teammates ELO: 1500
 * Opponents' ELOs: 1600 and 1700

Your share is: 1850 / (1850 + 1500) ~ **0,55**%

If you won, it's treated like a win against an opponent with (1600 + 1700) * 0.55 ~ **1822** ELO.
If you lose, it's treated like a loss against (1600 + 1700) * (1.0 - 0.55) ~ **1478** ELO.

It might seem counterintuitive that the better player is penalized more for a loss, since it might not be their fault. But if we give all credit for wins to the better player and place all blame for losses on the weaker one, the system breaks down - ratings drift further apart. Since the better player tends to win more games in the long run, the penalty makes sense to keep the system balanced.

To improve results even further, we raise your ELO and your teammate's ELO to the power of a specific value _p_, which adjusts the weight each player has:

   yourStrength = pow(yourElo, *p*) ~ 4232
   yourTeammatesStrength = pow(teammatesElo, *p*) ~ 3353

   Your share becomes: 4232 / (4232 + 3353) ~ **0.56**%


To find the optimal _p_, we ran many simulations, updating ELOs daily and comparing predicted vs. actual outcomes for various team ELO differences. A _p_ of 1.00 corresponds to the original percentage method. This already performs better than the naive win/loss splitting:


| Difference Team Elo | Expected | 0.70  | 0.80  | 1.00  | 1.10  | 1.11  | 1.15  | 1.20  |
|---------------------|----------|-------|-------|-------|-------|-------|-------|-------|
| +0                  | 50.00    | 51.29 | 50.79 | 51.05 | 50.65 | 50.51 | 50.74 | 50.75 |
| +50                 | 57.15    | 57.32 | 56.98 | 56.94 | 57.33 | 57.12 | 57.31 | 57.38 |
| +100                | 64.01    | 64.80 | 64.74 | 64.63 | 64.50 | 64.47 | 64.26 | 63.95 |
| +150                | 70.34    | 72.87 | 72.50 | 71.54 | 71.69 | 71.59 | 71.83 | 71.76 |
| +200                | 75.97    | 76.61 | 76.41 | 75.60 | 75.14 | 75.54 | 75.04 | 74.45 |
| +250                | 80.83    | 82.18 | 82.42 | 82.02 | 80.75 | 80.81 | 80.51 | 80.66 |
| +300                | 84.90    | 85.01 | 85.52 | 83.98 | 83.95 | 84.56 | 85.03 | 84.62 |
| +350                | 88.23    | 90.37 | 90.49 | 87.42 | 87.24 | 86.78 | 86.94 | 84.60 |
| +400                | 90.91    | 92.40 | 91.14 | 89.16 | 88.36 | 88.41 | 88.50 | 88.12 |
| +450                | 93.02    | 93.42 | 94.38 | 94.60 | 92.60 | 92.48 | 90.80 | 88.05 |
| +500                | 94.68    | 96.64 | 96.32 | 94.19 | 93.33 | 93.20 | 94.12 | 95.91 |
| **Average Error**   |          | 1.17  | 1.09  |  0.93 |  0.89 |  0.82 |  0.95 |  1.55 |

The sweet spot is _p_ = **1.11**, giving reasonably good results in line with expectations.

## Usage on CnCNet

Since ELO computation is processed in batches - to ensure higher accuracy - rather than after each game, this application needs to run once per day. The "ELO day" ends at UTC+5 for all players, so the best time to run it is between UTC+5 and UTC+6.

During the build of the Docker container, this repository is cloned and the app is compiled.

### Build the container:

```bash
docker build -t elogenerator .
```

If packages cannot be fetched, change

  RUN apt-get update && apt-get install -y \

to

  RUN apt-get -o Acquire::ForceIPv4=true update && apt-get -o Acquire::ForceIPv4=true install -y \

in the `Dockerfile`.

When running the container, Docker must be provided with the path to the directory containing the rating files.
This is usually:

`cncnet-ladder-api/cncnet-api/storage/app/rating`

The ELO generator will use this directory as **output directory**.

You can provide the environment variables `MYSQL_HOST`, `MYSQL_PASSWORD`, `MYSQL_USER`, and `MYSQL_PORT` - but these can also be passed as command-line arguments to the ELO generator app.

### Running the container:

**Command-line parameters:**

  * `-H` or `--host`: MySql host name, will be preferred over environment variable MYSQL_HOST
  * `-u` or `--user`: MySql user, will be preferred over environment variable MYSQL_USER
  * `-P` or `--password`: MySql password, will be preferred over environment variable MYSQL_PASSWORD
  * `-p` or `--port`: MySql port, will be preferred over environment variable MYSQL_PORT
  * `-m` or `--gamemode`: blitz, blitz-2v2, ra2, ra and yr are tested, other might work too. ra2 will include ra2-new-maps
  * `-o` or `--output-dir`: Location where all JSON files are written. Needs to align with the directory provided with the docker container
  * `-l` or `--log-level`: Defaults to verbose, but if files get too large, info or warning might be the better choice.
  * `-t` or `--tournament-games`: File with additional tournament games. There is one for blitz.
  * `-s` or `--statistics`: Will create individual statistics for each player. Quite fast, but will eat up a lot of disk storage.


### Example 1:

```bash
docker run -it --add-host=host.docker.internal:host-gateway -e MYSQL_PASSWORD=pass -e MYSQL_USER=user -v /home/cncnet/cncnet-ladder-api/cncnet-api/storage/app/rating:/data elogenerator -H host.docker.internal -P 3306 --log-level info -o /data -m ra2
```

This will generate ELO for `ra2` and write the JSON files to `/home/cncnet/cncnet-ladder-api/cncnet-api/storage/app/rating`.
It connects to MySQL at the given host and port. User and password are provided via environment variables.


### Example 2:

```bash
docker run -it --add-host=host.docker.internal:host-gateway -e MYSQL_HOST=host.docker.internal -e MYSQL_PORT=3307 -e MYSQL_PASSWORD=pass -e MYSQL_USER=user -v /home/cncnet/cncnet-ladder-api/cncnet-api/storage/app/rating:/data elogenerator -o /data -m blitz --tournament-games blitz_ra2_worldseries.json
```

This will generate ELO for blitz, using environment variables only. It also includes the specified tournament game file. Since a tournament file is only used for blitz and not updated anymore, the layout of a tournament won't be explained here. Just throw `gameoverlay.cpp` at an AI and it will provide you an example JSON.


## FAQ

_This section is still being created._

### Why do I have several ratings? What's my actual ELO?

You have **separate ratings for each faction** - Soviet, Allied, and (if available) Yuri — **plus an overall rating** that includes all your games, regardless of faction. This separation helps track your true skill with each faction, since performance can vary significantly depending on what you play.

Your actual ELO is the **highest of these ratings** - but only if its deviation is low enough to qualify you as an active player. If not, the next-highest active rating will be shown instead.

---

### What is the deviation?

The **deviation** indicates how accurate or uncertain your rating is. You’ll appear on the list of **active players** if your deviation drops below **60–70** (depending on your rating), and you'll be removed from the list if it rises above around **100**.

To lower your deviation, you need to provide **new information** about your skill level. Continuously beating much weaker opponents (which might boost your rating in other systems) doesn't help here - it's not new information. For example, you’re expected to win more than **99%** of the time against someone **1000 ELO** below you. So winning 20 games in a row against such an opponent won’t improve your rating - it might even **increase** your deviation due to the lack of meaningful data.

That’s one reason why Glicko-2 is hard to exploit. The best way to reduce your deviation is to play a variety of opponents **close to your skill level**.

---

### How many points do I gain or lose per game?

**Short answer:** It depends.

Games are submitted to the system in **daily batches**, not individually. Your rating is updated based on your **overall performance** for the day, rather than on a per-game basis. This method provides a more accurate and stable rating.

---

### I didn’t care about my rating before, but now I want to take it seriously. Can you reset it?

**No.** Your rating reflects your performance over roughly the **last 200–300 games**. Older games gradually lose their impact.

If you play **50 games a day for a week**, your rating will effectively reset itself through recent performance. The result will be the same as starting from scratch - just without creating a new account.

---

### How bad is a loss against a much weaker opponent?

A single loss isn’t a big deal - even if your opponent is far below you in rating. The expected win rate is never 100%, no matter how large the ELO gap is. So if you lose one game against someone **1000 ELO** below you, it's likely just the one game out of 100 that you were expected to lose anyway.

In **traditional ELO systems**, a loss like that would often cause a **big drop**, which you'd have to grind back through many small wins. But Glicko-2 works differently: after an unexpected result, it usually adjusts your **deviation** first - not your rating. That means the system becomes less certain about your skill, but doesn’t immediately assume you've gotten worse.

Only if you continue to underperform - say, by losing a second or third time - does the system start lowering your rating. This makes Glicko-2 less punishing for flukes and more accurate.

---

### How can I improve my rating?

The short answer: **Play well.**

One thing to avoid: **going on tilt**. A string of unexpected losses - particularly against much weaker players will significantly drop your rating. Glicko-2 reacts strongly to sudden instability, unlike traditional ELO systems which only consider win/loss and not confidence.

**Consistency beats streakiness.** Glicko-2 favors players who stay sharp.

---

### How often is the list updated?

The rankings for **Blitz 2v2**, **YR**, and **RA2** are updated once per day, with a daily cutoff at UTC+5 (which corresponds to **midnight EST**). Your rating - as shown on the all-time leaderboard - reflects your status at the end of the previous day, based on that cutoff.

Due to lower activity, **Blitz** and **RA1** rankings are updated less frequently - about every second day.

---

### What happens if I stop playing?

If you become inactive, your **rating stays the same**, but your **deviation slowly increases** over time. That means the system becomes less certain about your skill. After a while, you’ll drop off the list of active players until you return and play enough games to reduce your deviation again.

---

### How can set my alias?

Asked an admin on Discord.

---

### My rating changed even though I didn’t play. What happened?

Occasionally, ratings may be adjusted due to external corrections - for example:

  - Invalid games being removed.
  - Duplicate or smurf accounts being detected and cleaned up or merged.

---

### I think a player is boosting their rating by playing the same opponent over and over. What do you do about it?

Glicko-2 is designed to **ignore repetitive or uninformative results**. Beating the same opponent over and over - especially if they’re much weaker - has little to no effect after a while.

The system also puts **less weight on games against highly uncertain players**, such as new or inactive accounts. If your opponent’s own rating isn’t reliable, the result doesn’t say much about your own strength - and the system treats it accordingly.

In short:

  - **Farming one player doesn’t work.**
  - **Games against unknown players have little to no effect.**

If you want to climb, you’ll need to prove your skill against a variety of active, stable opponents.

# Version history

### 1.0.0

- Initial version

### 1.0.1

- Sets day change to UTC+9

### 1.0.2

- Minor timezone fix

### 1.0.3

- Write ratings to database (user_ratings)

### 1.0.4

- Uses confirmed duplicates from CnCNet by default, can still use old algorithm to detect duplicates or ignore duplicates at all

### 1.0.5

- Performance improvements (bundling SQL queries & C++ optimizations)

### 1.0.06

- Added support for RA2 2v2 game mode
