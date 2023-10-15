#include "VpcMidiDisplay.hpp"
#include <ui/MenuSeparator.hpp>
#include <helpers.hpp>


namespace ptone {


    rack::midi::Driver* IoPort::getDriver() {
        return input->getDriver();
    }
    int IoPort::getDriverId() {
        return input->getDriverId();
    }
    void IoPort::setDriverId(int driverId) {
        input->setDriverId(driverId);
        output->setDriverId(driverId);
    }
    
    rack::midi::Device* IoPort::getDevice() {
        return input->getDevice();
    }
    std::vector<int> IoPort::getDeviceIds() {
        return input->getDeviceIds();
    }
    int IoPort::getDeviceId() {
        return input->getDeviceId();
    }
    void IoPort::setDeviceId(int deviceId) {
        input->setDeviceId(deviceId);
        output->setDeviceId(deviceId);
    }
    std::string IoPort::getDeviceName(int deviceId) {
        return input->getDeviceName(deviceId);
    }


struct MidiDriverValueItem : rack::ui::MenuItem {
	IoPort* port;
	int driverId;
	void onAction(const ActionEvent& e) override {
		port->setDriverId(driverId);
	}
};

static void appendMidiDriverMenu(rack::ui::Menu* menu, IoPort* port) {
	if (!port)
		return;

	for (int driverId : rack::midi::getDriverIds()) {
		MidiDriverValueItem* item = new MidiDriverValueItem;
		item->port = port;
		item->driverId = driverId;
		item->text = rack::midi::getDriver(driverId)->getName();
		item->rightText = CHECKMARK(item->driverId == port->getDriverId());
		menu->addChild(item);
	}
}

void MidiDriverChoice::onAction(const ActionEvent& e) {
	rack::ui::Menu* menu = rack::createMenu();
	menu->addChild(rack::createMenuLabel("MIDI driver"));
	appendMidiDriverMenu(menu, port);
}

void MidiDriverChoice::step() {
	text = (port && port->input->driver) ? port->getDriver()->getName() : "";
	if (text.empty()) {
		text = "(No driver)";
		color.a = 0.5f;
	}
	else {
		color.a = 1.f;
	}
}

struct MidiDriverItem : rack::ui::MenuItem {
	IoPort* port;
	rack::ui::Menu* createChildMenu() override {
		rack::ui::Menu* menu = new rack::ui::Menu;
		appendMidiDriverMenu(menu, port);
		return menu;
	}
};


struct MidiDeviceValueItem : rack::ui::MenuItem {
	IoPort* port;
	int deviceId;
	void onAction(const ActionEvent& e) override {
		port->setDeviceId(deviceId);
	}
};

static void appendMidiDeviceMenu(rack::ui::Menu* menu, IoPort* port) {
	if (!port)
		return;

	{
		MidiDeviceValueItem* item = new MidiDeviceValueItem;
		item->port = port;
		item->deviceId = -1;
		item->text = "(No device)";
		item->rightText = CHECKMARK(item->deviceId == port->getDeviceId());
		menu->addChild(item);
	}

	for (int deviceId : port->getDeviceIds()) {
		MidiDeviceValueItem* item = new MidiDeviceValueItem;
		item->port = port;
		item->deviceId = deviceId;
		item->text = port->getDeviceName(deviceId);
		item->rightText = CHECKMARK(item->deviceId == port->getDeviceId());
		menu->addChild(item);
	}
}

void MidiDeviceChoice::onAction(const ActionEvent& e) {
	rack::ui::Menu* menu = rack::createMenu();
	menu->addChild(rack::createMenuLabel("MIDI device"));
	appendMidiDeviceMenu(menu, port);
}

void MidiDeviceChoice::step() {
	text = (port && port->input->device) ? port->getDevice()->getName() : "";
	if (text.empty()) {
		text = "(No device)";
		color.a = 0.5f;
	}
	else {
		color.a = 1.f;
	}
}

struct MidiDeviceItem : rack::ui::MenuItem {
	IoPort* port;
	rack::ui::Menu* createChildMenu() override {
		rack::ui::Menu* menu = new rack::ui::Menu;
		appendMidiDeviceMenu(menu, port);
		return menu;
	}
};


void MidiDisplay::setMidiPort(IoPort* port) {
	clearChildren();

    rack::math::Vec pos;

	MidiDriverChoice* driverChoice = rack::createWidget<MidiDriverChoice>(pos);
	driverChoice->box.size.x = box.size.x;
	driverChoice->port = port;
	addChild(driverChoice);
	pos = driverChoice->box.getBottomLeft();
	this->driverChoice = driverChoice;

	this->driverSeparator = rack::createWidget<rack::app::LedDisplaySeparator>(pos);
	this->driverSeparator->box.size.x = box.size.x;
	addChild(this->driverSeparator);

	MidiDeviceChoice* deviceChoice = rack::createWidget<MidiDeviceChoice>(pos);
	deviceChoice->box.size.x = box.size.x;
	deviceChoice->port = port;
	addChild(deviceChoice);
	pos = deviceChoice->box.getBottomLeft();
	this->deviceChoice = deviceChoice;

	this->deviceSeparator = rack::createWidget<rack::app::LedDisplaySeparator>(pos);
	this->deviceSeparator->box.size.x = box.size.x;
	addChild(this->deviceSeparator);

}


void MidiButton::setMidiPort(IoPort* port) {
	this->port = port;
}


void MidiButton::onAction(const ActionEvent& e) {
	rack::ui::Menu* menu = rack::createMenu();
	appendMidiMenu(menu, port);
}


void appendMidiMenu(rack::ui::Menu* menu, IoPort* port) {
	menu->addChild(rack::createMenuLabel("MIDI driver"));
	appendMidiDriverMenu(menu, port);

	menu->addChild(new rack::ui::MenuSeparator);
	menu->addChild(rack::createMenuLabel("MIDI device"));
	appendMidiDeviceMenu(menu, port);

	menu->addChild(new rack::ui::MenuSeparator);
	// menu->addChild(rack::createMenuLabel("MIDI channel"));
	// appendMidiChannelMenu(menu, port);

	// Uncomment this to use sub-menus instead of one big menu.

	// MidiDriverItem* driverItem = rack::createMenuItem<MidiDriverItem>("MIDI driver", RIGHT_ARROW);
	// driverItem->port = port;
	// menu->addChild(driverItem);

	// MidiDeviceItem* deviceItem = rack::createMenuItem<MidiDeviceItem>("MIDI device", RIGHT_ARROW);
	// deviceItem->port = port;
	// menu->addChild(deviceItem);

}

} //namespace ptone
