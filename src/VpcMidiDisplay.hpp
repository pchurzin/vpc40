#pragma once
#include <app/common.hpp>
#include <app/LedDisplay.hpp>
#include <ui/Menu.hpp>
#include <app/SvgButton.hpp>
#include <midi.hpp>

namespace ptone {

struct IoPort {
    rack::midi::Port* input;
    rack::midi::Port* output;

    rack::midi::Driver* getDriver();
    int getDriverId();
    void setDriverId(int driverId);
    
    rack::midi::Device* getDevice();
    std::vector<int> getDeviceIds();
    int getDeviceId();
    void setDeviceId(int deviceId);
    std::string getDeviceName(int deviceId);
};

struct MidiDriverChoice : rack::app::LedDisplayChoice {
	IoPort* port;
	void onAction(const ActionEvent& e) override;
	void step() override;
};


struct MidiDeviceChoice : rack::app::LedDisplayChoice {
	IoPort* port;
	void onAction(const ActionEvent& e) override;
	void step() override;
};

struct MidiDisplay : rack::app::LedDisplay {
	MidiDriverChoice* driverChoice;
    rack::app::LedDisplaySeparator* driverSeparator;
	MidiDeviceChoice* deviceChoice;
    rack::app::LedDisplaySeparator* deviceSeparator;
	void setMidiPort(IoPort* port);
};

/** A virtual MIDI port graphic that displays an MIDI menu when clicked. */
struct MidiButton : rack::app::SvgButton {
	IoPort* port;
	void setMidiPort(IoPort* port);
	void onAction(const ActionEvent& e) override;
};


/** Appends menu items to the given menu with driver, device, etc.
Useful alternative to putting a MidiDisplay on your module's panel.
*/
void appendMidiMenu(rack::ui::Menu* menu, IoPort* port);
}; //namespace ptone
