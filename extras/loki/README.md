# Loki и Grafana рядом с платой

Готовый контейнер: журнал с платы складывается в Loki, а смотрят его в
Grafana. Настраивать ничего не нужно — источник данных прописан заранее.

---

## Что нужно

**Linux.** Docker из репозитория дистрибутива:

```bash
sudo apt install docker.io docker-compose-v2     # Debian, Ubuntu
sudo dnf install docker docker-compose-plugin    # Fedora
sudo systemctl enable --now docker
sudo usermod -aG docker $USER                    # чтобы не писать sudo
```

Последняя команда подействует после нового входа в систему.

**Windows.** [Docker Desktop](https://www.docker.com/products/docker-desktop/) —
установщик всё сделает сам, включая WSL 2. После установки его нужно запустить:
контейнеры работают, только пока он открыт.

---

## Запуск

Из этого каталога:

```bash
docker compose up -d
```

Первый раз образы качаются несколько минут, дальше — секунды.

Проверить, что поднялось:

```bash
docker compose ps
```

Оба контейнера должны быть `running`.

---

## Куда смотреть

Grafana — <http://localhost:3000>. Пароль не спрашивается.

Журнал: слева **Explore**, источник **Loki**, и запрос в строке:

```
{job="boiler"}                      всё с этой платы
{job="boiler", level="error"}       только ошибки
{job="boiler"} |= "pump"            строки со словом pump
{job="boiler", source="net"}        только от источника net
```

Метки `level` и `source` библиотека ставит сама, `job` и `instance` задаёт
приложение через `loki::addLabel()`.

---

## Что указать плате

Адрес машины, где поднят контейнер, — не `localhost`: для платы это она сама.

```cpp
loki::begin("192.168.1.10");   // порт 3100 по умолчанию
loki::addLabel("job", "boiler");
```

Свой адрес в сети:

```bash
ip -4 addr | grep inet          # Linux
ipconfig                        # Windows
```

---

## Остановить

```bash
docker compose down             # остановить, журнал сохранится
docker compose down -v          # и удалить накопленное
```

---

## Если строки не появляются

Дошли ли они до Loki — видно по нему самому, без Grafana:

```bash
curl -s "http://localhost:3100/loki/api/v1/label/job/values"
```

Ответ вроде `{"status":"success","data":["boiler"]}` означает, что плата
достучалась, и разбираться нужно с запросом в Grafana. Пустой `data` — строки
не дошли:

- **плата не видит машину** — проверьте адрес и что они в одной сети;
- **мешает межсетевой экран** — на Windows Docker Desktop просит разрешение
  при первом запуске, его легко пропустить; на Linux порт 3100 может закрывать
  `ufw` или `firewalld`;
- **строки ещё в буфере** — плата копит их пачкой и шлёт раз в несколько
  секунд; `loki::flush()` отправляет немедленно;
- **строки теряются** — `loki::lost()` покажет сколько, а `loki::connected()`
  — дошла ли последняя пачка.

Что происходит внутри контейнеров:

```bash
docker compose logs loki
docker compose logs grafana
```

---

## Проверить без платы

Строку можно отправить руками — так проверяется вся цепочка, пока прошивки
ещё нет:

```bash
curl -H "Content-Type: application/json" -XPOST \
  "http://localhost:3100/loki/api/v1/push" \
  --data-raw "{\"streams\":[{\"stream\":{\"job\":\"boiler\",\"level\":\"info\"},\"values\":[[\"$(date +%s)000000000\",\"hello from curl\"]]}]}"
```

В Grafana по запросу `{job="boiler"}` она появится сразу.

На Windows ту же команду удобнее дать из PowerShell:

```powershell
$ns = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds().ToString() + "000000000"
$body = "{`"streams`":[{`"stream`":{`"job`":`"boiler`",`"level`":`"info`"},`"values`":[[`"$ns`",`"hello from powershell`"]]}]}"
Invoke-RestMethod -Method Post -Uri "http://localhost:3100/loki/api/v1/push" `
  -ContentType "application/json" -Body $body
```
