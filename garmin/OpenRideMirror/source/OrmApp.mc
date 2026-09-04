// SPDX-License-Identifier: GPL-3.0-only
using Toybox.Application as Application;

class OrmApp extends Application.AppBase {
    private var _transport;

    function initialize() {
        AppBase.initialize();
        _transport = new OrmBleTransport();
    }

    function onStart(state) {
        _transport.start();
    }

    function onStop(state) {
        _transport.stop();
    }

    function getInitialView() {
        return [new OrmField(_transport)];
    }
}
