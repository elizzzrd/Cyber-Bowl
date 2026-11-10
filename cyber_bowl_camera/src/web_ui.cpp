#include "web_ui.hpp"

const char CAMERA_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="ru">
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <title>Cyber Bowl · Камера</title>
        <style>
            :root {
                color-scheme: dark;
                font-family: system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;
                background: #09151d;
                color: #ecf3f7;
            }

            * {
                box-sizing: border-box;
            }

            body {
                margin: 0;
            }

            main {
                max-width: 1600px;
                margin: auto;
                padding: 24px 18px 44px;
            }

            header {
                display: flex;
                align-items: center;
                justify-content: space-between;
                gap: 16px;
                margin-bottom: 24px;
            }

            .brand {
                font-size: 12px;
                letter-spacing: .15em;
                color: #65c7ea;
                font-weight: 750;
            }

            h1 {
                font-size: 30px;
                letter-spacing: -.04em;
                margin: 7px 0;
            }

            h2 {
                font-size: 18px;
                margin: 0 0 18px;
            }

            .muted,
            small {
                color: #9cabb5;
            }

            small {
                font-size: 12px;
                line-height: 1.6;
            }

            .badge {
                border: 1px solid #29404e;
                border-radius: 30px;
                padding: 8px 12px;
                font-size: 12px;
                white-space: nowrap;
            }

            .grid {
                display: grid;
                grid-template-columns: minmax(0,1fr) 340px;
                gap: 18px;
            }

            .card {
                background: #11232e;
                border: 1px solid #243944;
                border-radius: 18px;
                padding: 22px;
                margin-bottom: 18px;
            }

            .viewer {
                aspect-ratio: 4/3;
                background: #050c12;
                border: 1px solid #29404e;
                border-radius: 12px;
                display: grid;
                place-items: center;
                overflow: hidden;
                position: relative;
            }

            .viewer img {
                position: absolute;
                inset: 0;
                display: none;
                width: 100%;
                height: 100%;
                object-fit: contain;
            }

            .placeholder {
                text-align: center;
                color: #8498a5;
                padding: 20px;
            }

            .placeholder b {
                display: block;
                color: #b9cbd5;
                margin-bottom: 8px;
                font-size: 20px;
            }

            button,
            .button {
                border: 1px solid #36525f;
                background: #1b3543;
                color: #f5fbff;
                border-radius: 9px;
                padding: 11px 15px;
                font: inherit;
                font-size: 14px;
                cursor: pointer;
                text-decoration: none;
                display: inline-block;
            }

            button:hover,
            .button:hover {
                background: #264555;
            }

            button.primary {
                background: #50bade;
                color: #06151d;
                border: 0;
                font-weight: 700;
            }

            button:disabled {
                opacity: .5;
                cursor: wait;
            }

            .actions {
                display: flex;
                gap: 8px;
                flex-wrap: wrap;
                margin: 16px 0 12px;
            }

            label {
                display: block;
                font-size: 13px;
                margin-top: 14px;
                margin-bottom: 6px;
                color: #c1d1d9;
            }

            input,
            select {
                width: 100%;
                border: 1px solid #35505f;
                border-radius: 8px;
                background: #0b1b25;
                color: #eff7fb;
                padding: 11px;
                font: inherit;
                font-size: 14px;
            }

            input[type=range] {
                padding: 8px 0;
                border: 0;
                accent-color: #54c3e6;
            }

            input[type=checkbox] {
                width: auto;
                accent-color: #54c3e6;
            }

            label.check {
                display: flex;
                gap: 9px;
                align-items: center;
                margin-top: 14px;
            }

            .two {
                display: grid;
                grid-template-columns: 1fr 1fr;
                gap: 12px;
            }

            output {
                float: right;
                color: #67ccec;
                font-variant-numeric: tabular-nums;
            }

            .info {
                display: grid;
                grid-template-columns: 1fr auto;
                gap: 9px;
                font-size: 13px;
                line-height: 1.6;
            }

            .info span:nth-child(odd) {
                color: #94a9b7;
            }

            .info span:nth-child(even) {
                text-align: right;
                max-width: 250px;
                overflow-wrap: anywhere;
            }

            .notice {
                border-left: 3px solid #fdaf63;
                padding: 10px 12px;
                font-size: 13px;
                line-height: 1.65;
                background: #20313a;
                border-radius: 0 8px 8px 0;
                margin-top: 16px;
            }

            #message {
                min-height: 28px;
                margin: 12px 0;
                color: #7cdaaf;
                font-size: 14px;
                line-height: 1.6;
                overflow-wrap: anywhere;
            }

            .links a {
                color: #69c9e9;
                overflow-wrap: anywhere;
            }

            details summary {
                cursor: pointer;
                color: #b4cbd7;
                font-size: 13px;
            }

            .help {
                margin-top: 22px;
                font-size: 12px;
                line-height: 1.7;
                color: #8ea4b2;
            }

            .viewer:fullscreen,
            .viewer.expanded {
                width: 100%;
                height: 100%;
                aspect-ratio: auto;
                border: 0;
                border-radius: 0;
                background: #000;
            }

            .viewer.expanded {
                position: fixed;
                inset: 0;
                width: 100vw;
                height: 100vh;
                height: 100dvh;
                z-index: 1000;
            }

            body.viewerExpanded {
                overflow: hidden;
            }

            .fullscreenExit {
                display: none;
                position: absolute;
                top: 16px;
                right: 16px;
                z-index: 2;
                background: #10232ee6;
            }

            .viewer:fullscreen .fullscreenExit,
            .viewer.expanded .fullscreenExit {
                display: block;
            }

            #message:empty {
                display: none;
            }

            .diagnostics summary {
                font-size: 18px;
                font-weight: 700;
                color: #ecf3f7;
            }

            .diagnostics[open] summary {
                margin-bottom: 18px;
            }

            @media (max-width:900px) {
                .grid {
                    grid-template-columns: 1fr;
                }

                main {
                    padding: 18px 12px;
                }

                .card {
                    padding: 18px;
                }

                h1 {
                    font-size: 27px;
                }

                .badge {
                    font-size: 11px;
                    padding: 7px 9px;
                }

            }
        </style>
    </head>
    <body>
        <main>
            <header>
                <div>
                    <div class="brand">CYBER BOWL</div>
                    <h1>Камера кормушки</h1>
                </div>
                <div class="badge" id="badge">Подключение…</div>
            </header>
            <div class="grid">
                <section>
                    <div class="card">
                        <div class="viewer" id="viewer">
                            <img id="video" alt="Видео с камеры">
                            <div class="placeholder" id="placeholder">
                                <b>Камера готова к просмотру</b>
                            </div>
                            <button class="fullscreenExit" id="fullscreenExit" aria-label="Выйти из полноэкранного режима">Закрыть ×</button>
                        </div>
                        <div class="actions">
                            <button class="primary" id="play">Смотреть видео</button>
                            <a class="button" href="/snapshot" download="cyber-bowl.jpg" id="snapshot">Снимок</a>
                            <button id="light">Свет на 30 с</button>
                            <button id="fullscreen" aria-controls="viewer" aria-pressed="false">На весь экран</button>
                        </div>
                        <div id="message" role="status" aria-live="polite">
                        </div>
                    </div>
                    <div class="card">
                        <details class="diagnostics">
                            <summary>Состояние</summary>
                            <div class="info">
                                <span>Камера</span>
                                <span id="cameraStatus">…</span>
                                <span>Домашняя сеть</span>
                                <span id="networkStatus">…</span>
                                <span>IP в домашней сети</span>
                                <span id="stationIp">…</span>
                                <span>Сеть самой камеры</span>
                                <span id="apName">…</span>
                                <span>Сигнал Wi-Fi</span>
                                <span id="rssi">…</span>
                                <span>PSRAM</span>
                                <span id="psram">…</span>
                                <span>Время работы</span>
                                <span id="uptime">…</span>
                            </div>
                            <div class="notice links" id="addressHint">Адреса появятся после подключения.</div>
                        </details>
                    </div>
                </section>
                <section>
                    <div class="card">
                        <h2>Изображение</h2>
                        <form id="imageForm">
                            <label for="size">Разрешение</label>
                            <select id="size" name="size">
                                <option value="0">320 × 240</option>
                                <option value="1">640 × 480</option>
                                <option value="2">800 × 600</option>
                            </select>
                            <label for="quality">
                                JPEG: меньше число — лучше
                                <output id="qualityOut">12</output>
                            </label>
                            <input id="quality" name="quality" type="range" min="10" max="40" value="12">
                            <label for="fps">
                                Предел кадров в секунду
                                <output id="fpsOut">10</output>
                            </label>
                            <input id="fps" name="fps" type="range" min="1" max="20" value="10">
                            <label for="brightness">
                                Яркость
                                <output id="brightnessOut">0</output>
                            </label>
                            <input id="brightness" name="brightness" type="range" min="-2" max="2" value="0">
                            <label for="contrast">
                                Контраст
                                <output id="contrastOut">0</output>
                            </label>
                            <input id="contrast" name="contrast" type="range" min="-2" max="2" value="0">
                            <label for="saturation">
                                Насыщенность
                                <output id="saturationOut">0</output>
                            </label>
                            <input id="saturation" name="saturation" type="range" min="-2" max="2" value="0">
                            <label class="check">
                                <input type="checkbox" id="mirror" name="mirror">
                                Зеркально по горизонтали
                            </label>
                            <label class="check">
                                <input type="checkbox" id="flip" name="flip">
                                Перевернуть по вертикали
                            </label>
                            <div class="actions">
                                <button class="primary" type="submit">Применить и сохранить</button>
                            </div>
                        </form>
                    </div>
                    <div class="card">
                        <h2>Подключение к Wi-Fi</h2>
                        <form id="wifiForm">
                            <label for="ssid">Название домашней сети (2,4 ГГц)</label>
                            <input id="ssid" name="ssid" maxlength="32" autocomplete="off" placeholder="Название сети">
                            <label for="password">Пароль домашней сети</label>
                            <input id="password" name="password" type="password" maxlength="64" autocomplete="new-password" placeholder="Пусто: оставить сохранённый">
                            <label class="check">
                                <input type="checkbox" id="open" name="open">
                                Эта домашняя сеть без пароля
                            </label>
                            <details style="margin-top:18px">
                                <summary>Пароль сети самой камеры</summary>
                                <label for="apPassword">Новый пароль (8–63 символа)</label>
                                <input id="apPassword" name="apPassword" type="password" maxlength="63" autocomplete="new-password" placeholder="Пусто: оставить прежний">
                            </details>
                            <div class="actions">
                                <button class="primary" type="submit">Сохранить подключение</button>
                            </div>
                            <details>
                                <summary>Как подключиться</summary>
                                <small>При первой настройке подключитесь к сети CyberBowl-Camera-… и откройте 192.168.4.1. После подключения к роутеру переключите телефон в домашнюю сеть. Пустое название сети отключает подключение к роутеру.</small>
                            </details>
                        </form>
                    </div>
                </section>
            </div>
        </main>
        <script>
            const $ = id => document.getElementById(id);

            let playing = false;
            let initialised = false;
            let lightOn = false;
            let statusPending = false;

            function message(text, error = false) {
                $('message').textContent = text;
                $('message').style.color = error ? '#ff9b92' : '#7cdaaf';
            }

            function stopVideo() {
                playing = false;
                $('video').removeAttribute('src');
                $('video').style.display = 'none';
                $('placeholder').style.display = 'block';
                $('play').textContent = 'Смотреть видео';
            }

            $('play').addEventListener('click', () => {
                if (playing) {
                    stopVideo();
                    return;
                }

                playing = true;
                $('video').style.display = 'block';
                $('placeholder').style.display = 'none';
                $('play').textContent = 'Остановить видео';
                $('video').src = 'http://' + location.hostname + ':81/stream?t=' + Date.now();
            });

            // Keep native fullscreen and the in-page fallback in sync, including browser/Esc exits.
            function expanded() {
                return document.fullscreenElement === $('viewer') ||
                       $('viewer').classList.contains('expanded');
            }

            function syncFullscreen() {
                const active = expanded();
                $('fullscreen').textContent = active ? 'Выйти из полного экрана' : 'На весь экран';
                $('fullscreen').setAttribute('aria-pressed', String(active));
            }

            function closeExpanded() {
                const wasExpanded = $('viewer').classList.contains('expanded');

                $('viewer').classList.remove('expanded');
                document.body.classList.remove('viewerExpanded');
                syncFullscreen();

                if (wasExpanded) {
                    $('fullscreen').focus();
                }
            }

            async function toggleFullscreen() {
                if (document.fullscreenElement === $('viewer')) {
                    try {
                        await document.exitFullscreen();
                    } catch (e) {
                        message('Не удалось выйти из полного экрана. Нажмите Esc.', true);
                    }
                    return;
                }

                if ($('viewer').classList.contains('expanded')) {
                    closeExpanded();
                    return;
                }

                if ($('viewer').requestFullscreen) {
                    try {
                        await $('viewer').requestFullscreen();
                        syncFullscreen();
                        return;
                    } catch (e) {
                        /* Some mobile browsers deny or lack native fullscreen. */
                    }
                }

                $('viewer').classList.add('expanded');
                document.body.classList.add('viewerExpanded');
                syncFullscreen();
                $('fullscreenExit').focus();
            }

            $('fullscreen').addEventListener('click', toggleFullscreen);
            $('fullscreenExit').addEventListener('click', toggleFullscreen);

            $('viewer').addEventListener('dblclick', event => {
                if (!event.target.closest('button')) {
                    toggleFullscreen();
                }
            });

            document.addEventListener('fullscreenchange', syncFullscreen);

            document.addEventListener('keydown', event => {
                if (event.key === 'Escape' && $('viewer').classList.contains('expanded')) {
                    closeExpanded();
                }
            });

            $('video').addEventListener('error', () => {
                if (playing) {
                    stopVideo();
                    message('Поток недоступен. Проверьте питание и сеть; затем попробуйте снова.', true);
                }
            });

            for (const id of ['quality', 'fps', 'brightness', 'contrast', 'saturation']) {
                $(id).addEventListener('input', () => {
                    $(id + 'Out').textContent = $(id).value;
                });
            }

            async function api(path, data) {
                const controller = new AbortController();
                const timeout = setTimeout(() => controller.abort(), 9000);

                try {
                    const response = await fetch(path, {
                        method: data ? 'POST' : 'GET',
                        headers: data ? {
                            'Content-Type': 'application/x-www-form-urlencoded',
                            'X-CyberBowl': '1'
                        } : {},
                        body: data ? new URLSearchParams(data) : undefined,
                        signal: controller.signal,
                        cache: 'no-store'
                    });

                    const result = await response.json();
                    if (!response.ok) {
                        throw Error(result.error || 'Ошибка ' + response.status);
                    }
                    return result;
                } finally {
                    clearTimeout(timeout);
                }
            }

            function link(parent, caption, href) {
                const a = document.createElement('a');
                a.textContent = caption;
                a.href = href;
                parent.appendChild(a);
            }

            async function refresh() {
                if (statusPending) {
                    return;
                }
                statusPending = true;

                try {
                    const s = await api('/api/status');

                    $('badge').textContent = s.camera ? 'Камера работает' : 'Ошибка камеры';
                    $('cameraStatus').textContent = s.camera ? 'Готова' : s.cameraError;
                    $('networkStatus').textContent = s.connected ? s.ssid : 'Роутер не подключён';
                    $('stationIp').textContent = s.stationIp || 'Нет подключения';
                    $('apName').textContent = s.apName;
                    $('rssi').textContent = s.connected ? s.rssi + ' дБм' : '—';
                    $('psram').textContent = s.psram ? 'Доступна' : 'Нет: только 320 × 240';
                    $('uptime').textContent = Math.floor(s.uptime / 60) + ' мин';

                    lightOn = s.light;
                    $('light').textContent = lightOn ? 'Выключить свет' : 'Свет на 30 с';
                    $('play').disabled = !s.camera;
                    $('light').disabled = !s.camera;

                    const hint = $('addressHint');
                    hint.replaceChildren();
                    hint.append('Адрес в домашней сети: ');
                    if (s.stationIp) {
                        link(hint, 'http://' + s.stationIp + '/', 'http://' + s.stationIp + '/');
                    } else {
                        hint.append('ещё не получен');
                    }
                    hint.append(document.createElement('br'));
                    hint.append('Постоянное имя: ');
                    link(hint, 'cyberbowl-camera.local', 'http://cyberbowl-camera.local/');

                    if (!initialised) {
                        initialised = true;

                        for (const key of ['size', 'quality', 'fps', 'brightness', 'contrast', 'saturation']) {
                            $(key).value = s.image[key];
                            if ($(key + 'Out')) {
                                $(key + 'Out').textContent = s.image[key];
                            }
                        }

                        for (const key of ['mirror', 'flip']) {
                            $(key).checked = s.image[key];
                        }

                        $('ssid').value = s.ssid;
                        for (const option of $('size').options) {
                            option.disabled = !s.psram && option.value !== '0';
                        }
                    }
                } catch (e) {
                    $('badge').textContent = 'Нет связи';
                } finally {
                    statusPending = false;
                }
            }

            $('imageForm').addEventListener('submit', async event => {
                event.preventDefault();
                const button = event.submitter;
                button.disabled = true;

                const data = {};
                for (const key of ['size', 'quality', 'fps', 'brightness', 'contrast', 'saturation']) {
                    data[key] = $(key).value;
                }
                for (const key of ['mirror', 'flip']) {
                    data[key] = $(key).checked ? '1' : '0';
                }

                try {
                    await api('/api/image', data);
                    message('Параметры изображения сохранены.');
                } catch (e) {
                    message(e.message, true);
                } finally {
                    button.disabled = false;
                    refresh();
                }
            });

            $('wifiForm').addEventListener('submit', async event => {
                event.preventDefault();
                const button = event.submitter;
                button.disabled = true;

                try {
                    await api('/api/wifi', {
                        ssid: $('ssid').value,
                        password: $('password').value,
                        apPassword: $('apPassword').value,
                        open: $('open').checked ? '1' : '0'
                    });

                    $('password').value = '';
                    $('apPassword').value = '';
                    stopVideo();
                    message('Сеть сохранена. Подключение обновится через 1,5 с. При смене канала Wi-Fi телефон может потребовать повторного подключения.');
                } catch (e) {
                    message(e.message, true);
                } finally {
                    button.disabled = false;
                }
            });

            $('light').addEventListener('click', async () => {
                try {
                    await api('/api/light', { enabled: lightOn ? '0' : '1' });
                    refresh();
                } catch (e) {
                    message(e.message, true);
                }
            });

            refresh();
            setInterval(refresh, 3000);
        </script>
    </body>
</html>
)HTML";
