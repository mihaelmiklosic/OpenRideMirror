// SPDX-License-Identifier: GPL-3.0-only
using Toybox.Activity as Activity;
using Toybox.WatchUi as WatchUi;

class OrmField extends WatchUi.SimpleDataField {
    private var _transport;

    function initialize(transport) {
        SimpleDataField.initialize();
        _transport = transport;
        label = "ORM";
    }

    function compute(info) {
        var profile = null;

        try {
            profile = Activity.getProfileInfo();
        } catch (exception) {
            // Activity values remain useful even if a profile is unavailable.
        }

        try {
            _transport.updateActivity(info, profile);
            return _transport.getDisplayValue();
        } catch (exception) {
            // Keep the data field alive and retry next compute tick instead of
            // letting one unavailable Activity.Info member close the BLE app.
            return "DATA ERR";
        }
    }
}
