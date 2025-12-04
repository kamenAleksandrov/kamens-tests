# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0
import os
import re
import time
from typing import Callable, Tuple
from urllib import error, request

import pytest
from pytest_embedded import Dut
from pytest_embedded_idf.utils import idf_parametrize
# diff of esp32s2/esp32s3 ~45K, others ~50K

DIFF_THRESHOLD = {
    'esp32s2': 40 * 1000,
    'esp32s3': 40 * 1000,
    'default': 45 * 1000,
}


@pytest.mark.wifi_two_dut
@pytest.mark.parametrize('count, config, skip_autoflash', [(2, 'default|enable_softap', 'y')], indirect=True)
@idf_parametrize(
    'target',
    ['esp32', 'esp32c2', 'esp32c3', 'esp32s2', 'esp32s3', 'esp32c5', 'esp32c6', 'esp32c61'],
    indirect=['target'],
)
def test_wifi_sdkconfig_disable_softap_save_binary_size(
    dut: Tuple[Dut, Dut],
    log_performance: Callable[[str, object], None],
) -> None:
    # dut logs are not needed
    dut[0].serial.close()
    dut[1].serial.close()

    app_without_softap = dut[0].app
    app_with_softap = dut[1].app
    assert app_without_softap.sdkconfig['ESP_WIFI_SOFTAP_SUPPORT'] is False
    assert app_with_softap.sdkconfig['ESP_WIFI_SOFTAP_SUPPORT'] is True

    diff = os.path.getsize(app_with_softap.bin_file) - os.path.getsize(app_without_softap.bin_file)
    log_performance('wifi_disable_softap_save_bin_size', f'{diff} bytes')

    diff_threshold = DIFF_THRESHOLD.get(dut[0].target) or DIFF_THRESHOLD['default']
    assert diff > diff_threshold


def _wait_for_ip(dut: Dut, timeout: int = 90) -> str:
    match = dut.expect(re.compile(rb"Got IP: (\d+\.\d+\.\d+\.\d+)"), timeout=timeout)
    return match.group(1).decode()


def _http_request(url: str, data: bytes | None = None, method: str = 'GET', timeout: int = 10) -> str:
    req = request.Request(url, data=data, method=method)
    with request.urlopen(req, timeout=timeout) as resp:
        return resp.read().decode()


@pytest.fixture(scope='module')
def connected_device(dut: Dut) -> Tuple[Dut, str]:
    ip = _wait_for_ip(dut)
    base_url = f'http://{ip}'

    # The web server starts immediately after Wi-Fi connects, but give it a moment just in case.
    deadline = time.time() + 15
    while time.time() < deadline:
        try:
            _http_request(base_url + '/')
            break
        except (error.URLError, OSError):
            time.sleep(1)
    else:
        pytest.fail('Web server did not respond in time')

    return dut, ip


def test_wifi_connection(connected_device: Tuple[Dut, str]) -> None:
    _, ip = connected_device
    assert ip.count('.') == 3


def test_led_control_endpoints(connected_device: Tuple[Dut, str]) -> None:
    _, ip = connected_device
    base_url = f'http://{ip}'

    on_body = _http_request(base_url + '/led?state=on')
    assert 'LED turned ON' in on_body

    off_body = _http_request(base_url + '/led?state=off')
    assert 'LED turned OFF' in off_body


def test_storage_crud_endpoints(connected_device: Tuple[Dut, str]) -> None:
    _, ip = connected_device
    base_url = f'http://{ip}'

    _http_request(base_url + '/string', method='DELETE')

    empty_resp = _http_request(base_url + '/string')
    assert '(empty)' in empty_resp

    new_value = 'hello-from-test'
    _http_request(base_url + '/string', data=f'value={new_value}'.encode(), method='POST')

    read_back = _http_request(base_url + '/string')
    assert new_value in read_back

    _http_request(base_url + '/string', method='DELETE')


def test_web_server_root_page(connected_device: Tuple[Dut, str]) -> None:
    _, ip = connected_device
    base_url = f'http://{ip}'
    html = _http_request(base_url + '/')
    assert 'ESP32 LED and String Control' in html
