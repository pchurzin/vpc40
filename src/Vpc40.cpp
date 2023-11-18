#include "plugin.hpp"
#include "VpcMidiDisplay.hpp"
#include "vpc_protocol.hpp"

using namespace rack::midi;

struct VpcMidiDin : ptone::MidiButton {
	VpcMidiDin() {
		addFrame(Svg::load(asset::system("res/ComponentLibrary/MIDI_DIN.svg")));
		shadow->opacity = 0.0;
	}
};

struct Vpc40Module : Module {
    enum ParamIds {
        RESET_PARAM,
        TEST_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        NUM_INPUTS
    };
    enum OutputIds {
        DEVICE_KNOB_1_OUTPUT,
        DEVICE_KNOB_2_OUTPUT,
        DEVICE_KNOB_3_OUTPUT,
        DEVICE_KNOB_4_OUTPUT,
        DEVICE_KNOB_5_OUTPUT,
        DEVICE_KNOB_6_OUTPUT,
        DEVICE_KNOB_7_OUTPUT,
        DEVICE_KNOB_8_OUTPUT,
        TRACK_KNOB_1_OUTPUT,
        TRACK_KNOB_2_OUTPUT,
        TRACK_KNOB_3_OUTPUT,
        TRACK_KNOB_4_OUTPUT,
        TRACK_KNOB_5_OUTPUT,
        TRACK_KNOB_6_OUTPUT,
        TRACK_KNOB_7_OUTPUT,
        TRACK_KNOB_8_OUTPUT,
        TRACK_LEVEL_1_OUTPUT,
        TRACK_LEVEL_2_OUTPUT,
        TRACK_LEVEL_3_OUTPUT,
        TRACK_LEVEL_4_OUTPUT,
        TRACK_LEVEL_5_OUTPUT,
        TRACK_LEVEL_6_OUTPUT,
        TRACK_LEVEL_7_OUTPUT,
        TRACK_LEVEL_8_OUTPUT,
        MASTER_LEVEL_OUTPUT,
        X_FADER_OUTPUT,
        CUE_OUTPUT,
        LED_OUTPUT_1,
        LED_OUTPUT_2,
        LED_OUTPUT_3,
        LED_OUTPUT_4,
        LED_OUTPUT_5,
        LED_OUTPUT_6,
        LED_OUTPUT_7,
        LED_OUTPUT_8,
        NUM_OUTPUTS
    };
    enum LightIds {
        RESET_LIGHT,
        TEST_LIGHT,
        NUM_LIGHTS
    };

    InputQueue midiInput;
    rack::midi::Output midiOutput;
    ptone::IoPort ioPort;
    dsp::BooleanTrigger resetButtonTrigger;
    dsp::BooleanTrigger testButtonTrigger;
    dsp::Timer rateLimitTimer;
    float rateLimitPeriod = 1 / 200.f;
    uint8_t sysExDeviceId = -1;

    float device1 = 0.f;
    uint8_t bank = 0;
    bool bankChanged = true;

    // track knob values 
    float trackKnobVoltage[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    uint8_t trackKnobMidi[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    bool trackKnobUpdate[PORT_MAX_CHANNELS * C_KNOB_NUM] = {false};
    uint8_t trackKnobRingType[PORT_MAX_CHANNELS * C_KNOB_NUM] = {RING_TYPE_SINGLE};
    bool trackKnobRingTypeUpdate[PORT_MAX_CHANNELS * C_KNOB_NUM] = {false};
    // device knob values 
    float deviceKnobVoltage[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    uint8_t deviceKnobMidi[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    bool deviceKnobUpdate[PORT_MAX_CHANNELS * C_KNOB_NUM] = {false};
    uint8_t deviceKnobRingType[PORT_MAX_CHANNELS * C_KNOB_NUM] = {RING_TYPE_SINGLE};
    bool deviceKnobRingTypeUpdate[PORT_MAX_CHANNELS * C_KNOB_NUM] = {false};
    // volume faders
    float trackLevelVoltage[CHAN_NUM] = {0};
    // master level
    float masterLevelVoltage = 0.f;
    // x-fader
    float xFaderVoltage = 0.f;
    // cue
    uint8_t cueMidiValue = 0;
    float cueVoltage = 0.f;
    // track LEDs
    uint8_t trackLedMidiValue[CHAN_LED_NUM * CHAN_NUM] = {0};
    bool trackLedUpdated[CHAN_LED_NUM * CHAN_NUM] = {false};
    bool trackLedToggle[CHAN_LED_NUM * CHAN_NUM] = {false};
    // shift
    bool isShifted = false;

    Vpc40Module() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configButton(RESET_PARAM, "Reset");
        configButton(TEST_PARAM, "Test");
        for (int i = 0; i < C_KNOB_NUM; i++) {
            configOutput(DEVICE_KNOB_1_OUTPUT + i, string::f("Device %d", i + 1));
            outputs[DEVICE_KNOB_1_OUTPUT + i].channels = PORT_MAX_CHANNELS;
            configOutput(TRACK_KNOB_1_OUTPUT + i, string::f("Track %d", i + 1));
            outputs[TRACK_KNOB_1_OUTPUT + i].channels = PORT_MAX_CHANNELS;
        }
        for (int i = 0; i < CHAN_NUM; i++) {
            configOutput(TRACK_LEVEL_1_OUTPUT + i, string::f("Level %d", i + 1));
        }
        configOutput(MASTER_LEVEL_OUTPUT, "Master level");
        configOutput(X_FADER_OUTPUT, "X-Fader level");
        configOutput(CUE_OUTPUT, "Cue level");
        for (int i = 0; i < CHAN_NUM; i++) {
            configOutput(LED_OUTPUT_1 + i, string::f("Channel %d leds", i + 1));
        }
        ioPort.input = &midiInput;
        ioPort.output = &midiOutput;
    }

    void process(const ProcessArgs &args) override {
        bool rateLimitTriggered = (rateLimitTimer.process(args.sampleTime) > rateLimitPeriod);
        if(rateLimitTriggered) rateLimitTimer.time -= rateLimitPeriod;
        if(resetButtonTrigger.process(params[RESET_PARAM].getValue())) {
            inquireDevice();
        }
        if(testButtonTrigger.process(params[TEST_PARAM].getValue())) {
            testMidi(args);
            testLedRingType(args);
            testLedRing(args);
        }
        Message inboundMidi;
        while (midiInput.tryPop(&inboundMidi, args.frame)) {
            //DEBUG("Channel: %d, Status: %d, Note/CC: %d, Value: %d", inboundMidi.getChannel(), inboundMidi.getStatus(), inboundMidi.getNote(), inboundMidi.getValue());
            if (inboundMidi.bytes[0] == 0xF0 && 
                    inboundMidi.bytes[3] == 0x06 &&
                    inboundMidi.bytes[4] == 0x02) {
                processInquireResponse(inboundMidi);
                introduce();
                reset();
            }
            
            if (isNoteOn(inboundMidi)) {
                processNoteOn(inboundMidi);
            } else if (isNoteOff(inboundMidi)) {
                processNoteOff(inboundMidi);
            } else if (isCc(inboundMidi)) {
                processCc(inboundMidi);
            }
        }

        if (rateLimitTriggered) {
            for (int k = 0; k < C_KNOB_NUM; k++) {
                int ki = knobIndex(k, bank);
                if (bankChanged) {
                    // update device ringType
                    setCc(args.frame, 0, C_DEVICE_KNOB_RING_TYPE_1 + k, deviceKnobRingType[ki]);
                    // update device knobValue
                    setCc(args.frame, 0, C_DEVICE_KNOB_1 + k, deviceKnobMidi[ki]);
                    // update track ringType
                    setCc(args.frame, 0, C_TRACK_KNOB_RING_TYPE_1 + k, trackKnobRingType[ki]);
                    // update track knob values
                    setCc(args.frame, 0, C_TRACK_KNOB_1 + k, trackKnobMidi[ki]);
                } 
                if (deviceKnobUpdate[ki]) {
                    // update knobValue
                    setCc(args.frame, 0, C_DEVICE_KNOB_1 + k, deviceKnobMidi[ki]);
                    deviceKnobUpdate[ki] = false;
                } 
                if (deviceKnobRingTypeUpdate[ki]) {
                    setCc(args.frame, 0, C_DEVICE_KNOB_RING_TYPE_1 + k, deviceKnobRingType[ki]);
                    deviceKnobRingTypeUpdate[ki] = false;
                }
                if (trackKnobUpdate[ki]) {
                    // update knobValue
                    setCc(args.frame, 0, C_TRACK_KNOB_1 + k, trackKnobMidi[ki]);
                    trackKnobUpdate[ki] = false;
                }
                if (trackKnobRingTypeUpdate[ki]) {
                    setCc(args.frame, 0, C_TRACK_KNOB_RING_TYPE_1 + k, trackKnobRingType[ki]);
                    trackKnobRingTypeUpdate[ki] = false;
                }
            }
            bankChanged = false;
            for (uint8_t c = 0; c < CHAN_NUM; c++) {
                for (uint8_t l = 0; l < CHAN_LED_NUM; l++) {
                    int ledIndex = trackLedIndex(l, c);
                    if (!trackLedUpdated[ledIndex]) continue;
                    if (trackLedMidiValue[ledIndex] == LED_OFF) {
                        setLedOff(args.frame, c, LED_RECORD + l);
                    }
                    else {
                        setLedOn(args.frame, c, LED_RECORD + l, trackLedMidiValue[ledIndex]);
                    }
                    trackLedUpdated[ledIndex] = false;
                }
            }
        }

        for (uint8_t c = 0; c < PORT_MAX_CHANNELS; c++) {
            for(int k = 0; k < C_KNOB_NUM; k++) {
                // update track knob outputs
                int ki = knobIndex(k, c);
                if (outputs[TRACK_KNOB_1_OUTPUT + k].isConnected()) {
                    outputs[TRACK_KNOB_1_OUTPUT + k].setVoltage(trackKnobVoltage[ki], c);
                }
                // update device knob outputs
                if (outputs[DEVICE_KNOB_1_OUTPUT + k].isConnected()) {
                    outputs[DEVICE_KNOB_1_OUTPUT + k].setVoltage(deviceKnobVoltage[ki], c);
                }
            }
        }
        for(uint8_t t = 0; t < CHAN_NUM; t++) {
            if(outputs[TRACK_LEVEL_1_OUTPUT + t].isConnected()) {
                outputs[TRACK_LEVEL_1_OUTPUT + t].setVoltage(trackLevelVoltage[t]);
            }
        }
        for (uint8_t c = 0; c < CHAN_LED_NUM; c++) {
            for (uint8_t t = 0; t < CHAN_NUM; t++) {
                if (outputs[LED_OUTPUT_1 + t].isConnected()) {
                    int ledIndex = trackLedIndex(c, t);
                    if (trackLedMidiValue[ledIndex] == LED_OFF) {
                        outputs[LED_OUTPUT_1 + t].setVoltage(0.0f, c);
                    } else {
                        outputs[LED_OUTPUT_1 + t].setVoltage(10.0f, c);
                    }
                }
            }
        }
        if (outputs[MASTER_LEVEL_OUTPUT].isConnected()) {
            outputs[MASTER_LEVEL_OUTPUT].setVoltage(masterLevelVoltage);
        }
        if (outputs[X_FADER_OUTPUT].isConnected()) {
            outputs[X_FADER_OUTPUT].setVoltage(xFaderVoltage);
        }
        if (outputs[CUE_OUTPUT].isConnected()) {
            outputs[CUE_OUTPUT].setVoltage(cueVoltage);
        }
    }

    bool isNoteOn(Message &msg) {
        return msg.getStatus() == STATUS_NOTE_ON;
    }

    bool isNoteOff(Message &msg) {
        return msg.getStatus() == STATUS_NOTE_OFF;
    }

    bool isCc(Message &msg) {
        return msg.getStatus() == STATUS_CC;
    }

    void processNoteOn(Message &msg) {
        uint8_t note = msg.getNote();
        if (isTrackLed(note)) {
            processTrackLedOn(note, msg.getChannel());
        } else {
            switch(note) {
                case BTN_RIGHT:
                    processBtnRightOn();
                    break;
                case BTN_LEFT:
                    processBtnLeftOn();
                    break;
                case BTN_SHIFT:
                    processShiftOn();
                    break;
            }
        }
    }

    void processTrackLedOn(uint8_t note, uint8_t channel) {
        uint8_t led = note - LED_RECORD;
        int ledIndex = trackLedIndex(led, channel);
        if (isShifted) {
            trackLedToggle[ledIndex] = !trackLedToggle[ledIndex];
            return;
        }
        if (trackLedToggle[ledIndex]) {
            processTrackLedToggle(ledIndex);
        } else {
            processTrackLedMomentary(ledIndex);
        }
    }

    void processTrackLedToggle(int ledIndex) {
        if (trackLedMidiValue[ledIndex] == LED_ON) {
            trackLedMidiValue[ledIndex] = LED_OFF;
        } else {
            trackLedMidiValue[ledIndex] = LED_ON;
        }
        trackLedUpdated[ledIndex] = true;
    }

    void processTrackLedMomentary(int ledIndex) {
        trackLedMidiValue[ledIndex] = LED_ON;
        trackLedUpdated[ledIndex] = true;
    }

    void processBtnRightOn() {
        bank = bank + 1;
        if (bank > PORT_MAX_CHANNELS - 1) bank = 0;
        bankChanged = true;
    }

    void processBtnLeftOn() {
        if (bank == 0) {
            bank = PORT_MAX_CHANNELS - 1;
        } else {
            bank = bank - 1;
        }
        bankChanged = true;
    }

    void processShiftOn() {
        isShifted = true;
    }


    void processNoteOff(Message &msg) {
        uint8_t note = msg.getNote();
        if (isTrackLed(note)) {
            processTrackLedOff(note, msg.getChannel());
        } else {
            switch(note) {
                case BTN_SHIFT:
                    processShiftOff();
                    break;
            }
        }
    }

    void processTrackLedOff(uint8_t note, uint8_t channel) {
        uint8_t led = note - LED_RECORD;
        int ledIndex = trackLedIndex(led, channel);
        if (trackLedToggle[ledIndex]) return;
        trackLedMidiValue[ledIndex] = LED_OFF;
        trackLedUpdated[ledIndex] = true;
    }

    void processShiftOff() {
        isShifted = false;
    }

    void processCc(Message &msg) {
        uint8_t cc = msg.getNote();
        if (isDeviceKnob(cc)) {
            processDeviceKnob(cc, msg.getValue());
        } else if (isTrackKnob(cc)) {
            processTrackKnob(cc, msg.getValue());
        } else if (cc == C_TRACK_LEVEL) {
            processTrackLevel(msg.getChannel(), msg.getValue());
        } else if (cc == C_MASTER_LEVEL) {
            processMasterLevel(msg.getValue());
        } else if (cc == C_CROSSFADER) {
            processXFaderLevel(msg.getValue());
        } else if (cc == C_CUE_LEVEL) {
            processCueLevel(msg.getValue());
        }
    }

    void processDeviceKnob(uint8_t cc, uint8_t value) {
        int knob = cc - C_DEVICE_KNOB_1;
        int ki = knobIndex(knob, bank);
        if (isShifted) {
            processDeviceKnobRingType(ki, value);
        } else {
            processDeviceKnobValue(ki, value);
        }
    }

    void processDeviceKnobRingType(int knobIndex, uint8_t value) {
        if (value < deviceKnobMidi[knobIndex]) {
            switch (deviceKnobRingType[knobIndex]) {
                case RING_TYPE_SINGLE:
                    deviceKnobRingType[knobIndex] = RING_TYPE_PAN;
                    break;
                case RING_TYPE_VOLUME:
                    deviceKnobRingType[knobIndex] = RING_TYPE_SINGLE;
                    break;
                case RING_TYPE_PAN:
                    deviceKnobRingType[knobIndex] = RING_TYPE_VOLUME;
                    break;
            }
        } else {
            switch (deviceKnobRingType[knobIndex]) {
                case RING_TYPE_SINGLE:
                    deviceKnobRingType[knobIndex] = RING_TYPE_VOLUME;
                    break;
                case RING_TYPE_VOLUME:
                    deviceKnobRingType[knobIndex] = RING_TYPE_PAN;
                    break;
                case RING_TYPE_PAN:
                    deviceKnobRingType[knobIndex] = RING_TYPE_SINGLE;
                    break;
            }
        }
        deviceKnobRingTypeUpdate[knobIndex] = true;
        // we must reset knobValue on the device
        deviceKnobUpdate[knobIndex] = true;
    }

    void processDeviceKnobValue(int knobIndex, uint8_t value) {
        uint8_t oldMidiValue = deviceKnobMidi[knobIndex];
        uint8_t newMidiValue = value;
        if(oldMidiValue != newMidiValue) {
            deviceKnobVoltage[knobIndex] = calculateVoltage(value);; 
            deviceKnobMidi[knobIndex] = newMidiValue;
            deviceKnobUpdate[knobIndex] = true;
        }
    }

    void processTrackKnob(uint8_t cc, uint8_t value) {
        int knob = cc - C_TRACK_KNOB_1;
        int ki = knobIndex(knob, bank);
        if (isShifted) {
            processTrackKnobRingType(ki, value);
        } else {
            processTrackKnobValue(ki, value);
        }
    }

    void processTrackKnobRingType(int knobIndex, uint8_t value) {
        if (value < trackKnobMidi[knobIndex]) {
            switch (trackKnobRingType[knobIndex]) {
                case RING_TYPE_SINGLE:
                    trackKnobRingType[knobIndex] = RING_TYPE_PAN;
                    break;
                case RING_TYPE_VOLUME:
                    trackKnobRingType[knobIndex] = RING_TYPE_SINGLE;
                    break;
                case RING_TYPE_PAN:
                    trackKnobRingType[knobIndex] = RING_TYPE_VOLUME;
                    break;
            }
        } else {
            switch (trackKnobRingType[knobIndex]) {
                case RING_TYPE_SINGLE:
                    trackKnobRingType[knobIndex] = RING_TYPE_VOLUME;
                    break;
                case RING_TYPE_VOLUME:
                    trackKnobRingType[knobIndex] = RING_TYPE_PAN;
                    break;
                case RING_TYPE_PAN:
                    trackKnobRingType[knobIndex] = RING_TYPE_SINGLE;
                    break;
            }
        }
        trackKnobRingTypeUpdate[knobIndex] = true;
        // we must reset knobValue on the track
        trackKnobUpdate[knobIndex] = true;
    }

    void processTrackKnobValue(int knobIndex, uint8_t value) {
        uint8_t oldMidiValue = trackKnobMidi[knobIndex];
        uint8_t newMidiValue = value;
        if(oldMidiValue != newMidiValue) {
            trackKnobVoltage[knobIndex] = calculateVoltage(value);
            trackKnobMidi[knobIndex] = newMidiValue;
            trackKnobUpdate[knobIndex] = true;
        }
    }

    void processTrackLevel(uint8_t channel, uint8_t value) {
        uint8_t track = channel;
        trackLevelVoltage[track] = calculateVoltage(value);
    }

    void processMasterLevel(uint8_t value) {
        masterLevelVoltage = calculateVoltage(value);
    }

    void processXFaderLevel(uint8_t value) {
        xFaderVoltage = calculateVoltage(value);
    }

    void processCueLevel(uint8_t value) {
        if (value > 0 && value <= 0x3F && cueMidiValue < 127) {
            uint8_t availableDelta = 127 - cueMidiValue;
            if (value < availableDelta) {
                cueMidiValue += value;
            } else {
                cueMidiValue = 127;
            }
            cueVoltage = calculateVoltage(cueMidiValue);
        } else if (value >= 0x40 && value <= 0x7F && cueMidiValue > 0) {
            uint8_t normalizedDelta = 0x80 - value;
            if (normalizedDelta < cueMidiValue) {
                cueMidiValue -= normalizedDelta;
            } else {
                cueMidiValue = 0;
            }
            cueVoltage = calculateVoltage(cueMidiValue);
        }
    }

    int knobIndex(uint8_t knob, uint8_t bank) {
        return knob * PORT_MAX_CHANNELS + bank;
    }

    bool isDeviceKnob(uint8_t cc) {
        return cc >= C_DEVICE_KNOB_1 && cc <= C_DEVICE_KNOB_8;
    }

    bool isTrackKnob(uint8_t cc) {
        return cc >= C_TRACK_KNOB_1 && cc <= C_TRACK_KNOB_8;
    }

    bool isTrackLed(uint8_t note) {
        return note >= LED_RECORD && note <= LED_CLIP_LAUNCH_5;
    }

    int trackLedIndex(uint8_t note, uint8_t channel) {
        return channel * CHAN_LED_NUM + note;
    }

    float calculateVoltage(uint8_t midiValue) {
        return 10.f * clamp(midiValue / 127.f, 0.f, 1.f);
    }

    void setCc(int64_t frame, uint8_t midiChannel, uint8_t cc, uint8_t value) {
        Message msg;
        msg.setFrame(frame);
        msg.setChannel(midiChannel);
        msg.setNote(cc);
        msg.setStatus(STATUS_CC);
        msg.setValue(value);
        midiOutput.sendMessage(msg);
    }

    void setLedOn(int64_t frame, uint8_t midiChannel, uint8_t note) {
        setLedOn(frame, midiChannel, note, LED_ON);
    }

    void setLedOff(int64_t frame, uint8_t midiChannel, uint8_t note) {
        Message msg;
        msg.setFrame(frame);
        msg.setChannel(midiChannel);
        msg.setNote(note);
        msg.setStatus(STATUS_NOTE_OFF);
        msg.setValue(0);
        midiOutput.sendMessage(msg);
    }
    void setLedOn(int64_t frame, uint8_t midiChannel, uint8_t note, uint8_t ledValue) {
        Message msg;
        msg.setFrame(frame);
        msg.setChannel(midiChannel);
        msg.setNote(note);
        msg.setStatus(STATUS_NOTE_ON);
        msg.setValue(ledValue);
        midiOutput.sendMessage(msg);
    }
    void inquireDevice() {
        Message msg;
        msg.setSize(6);
        msg.bytes[0] = 0xF0;
        msg.bytes[1] = 0x7E;
        msg.bytes[2] = 0x00;
        msg.bytes[3] = 0x06;
        msg.bytes[4] = 0x01;
        msg.bytes[5] = 0xF7;
        midiOutput.sendMessage(msg);
    }

    void processInquireResponse(Message& msg) {
        //todo: fix the midi button
        midiOutput.setChannel(-1);
        sysExDeviceId = msg.bytes[13];
    }

    void introduce() {
        Message msg;
        msg.setSize(12);
        msg.bytes[0] = 0xF0;
        msg.bytes[1] = 0x47;
        msg.bytes[2] = sysExDeviceId;
        msg.bytes[3] = 0x73;
        msg.bytes[4] = 0x60;
        msg.bytes[5] = 0x00;
        msg.bytes[6] = 0x04;
        msg.bytes[7] = 0x42;
        msg.bytes[8] = 0x00;
        msg.bytes[9] = 0x01;
        msg.bytes[10] = 0x00;
        msg.bytes[11] = 0xF7;
        midiOutput.sendMessage(msg);
    }

	void onPortChange(const PortChangeEvent& e) override {
        if (e.connecting && e.type == rack::engine::Port::OUTPUT) {
            if (e.portId >= LED_OUTPUT_1 && e.portId <= LED_OUTPUT_8) {
                outputs[e.portId].channels = CHAN_LED_NUM;
            } else if (e.portId >= DEVICE_KNOB_1_OUTPUT && e.portId <= DEVICE_KNOB_8_OUTPUT) {
                outputs[e.portId].channels = PORT_MAX_CHANNELS;
            } else if (e.portId >= TRACK_KNOB_1_OUTPUT && e.portId <= TRACK_KNOB_8_OUTPUT) {
                outputs[e.portId].channels = PORT_MAX_CHANNELS;
            }
        }
    }

    void reset() {
        bank = 0;
        bankChanged = true;
        for (int k = 0; k < C_KNOB_NUM; k++) {
            for (int c = 0; c < PORT_MAX_CHANNELS; c++) {
                int ki = knobIndex(k, c);
                trackKnobVoltage[ki] = 0.f;
                trackKnobMidi[ki] = 0;
                trackKnobUpdate[ki] = true;
                trackKnobRingType[ki] = RING_TYPE_SINGLE;
                trackKnobRingTypeUpdate[ki] = true;
                deviceKnobVoltage[ki] = 0.f;
                deviceKnobMidi[ki] = 0;
                deviceKnobUpdate[ki] = true;
                deviceKnobRingType[ki] = RING_TYPE_SINGLE;
                deviceKnobRingTypeUpdate[ki] = true;
            }
        }
    }

    void testMidi(const ProcessArgs& args) {
        Message msg;
        msg.setFrame(args.frame);
        msg.setStatus(0x09);
        msg.setChannel(0);
        msg.setNote(LED_RECORD);
        msg.setValue(LED_ON);
        midiOutput.sendMessage(msg);
    }

    void testLedRingType(const ProcessArgs& args) {
        Message msg;
        msg.setFrame(args.frame);
        msg.setStatus(0x0B);
        msg.setNote(C_DEVICE_KNOB_RING_TYPE_1);
        msg.setValue(RING_TYPE_PAN);
        midiOutput.sendMessage(msg);
    }
    void testLedRing(const ProcessArgs& args) {
        Message msg;
        msg.setFrame(args.frame);
        msg.setStatus(0x0B);
        msg.setNote(C_DEVICE_KNOB_1);
        msg.setValue(64);
        midiOutput.sendMessage(msg);
    }
};

struct Vpc40Widget : ModuleWidget {
    Vpc40Widget(Vpc40Module* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/panel.svg")));
        box.size = Vec(RACK_GRID_WIDTH * 38,RACK_GRID_HEIGHT);
        // MidiButton example
        ptone::MidiButton* midiButton = createWidget<VpcMidiDin>(Vec(0, 0));
        midiButton->setMidiPort(module ? &module->ioPort : NULL);
        addChild(midiButton);

        addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(101.441, 33.679)), module, Vpc40Module::RESET_PARAM, Vpc40Module::RESET_LIGHT));
        addParam(createLightParamCentered<VCVLightButton<MediumSimpleLight<WhiteLight>>>(mm2px(Vec(101.441, 63.679)), module, Vpc40Module::TEST_PARAM, Vpc40Module::TEST_LIGHT));

        for(int i = 0; i < C_KNOB_NUM / 2; i++) {
            addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(150 + 10 * i, 10)), module, Vpc40Module::TRACK_KNOB_1_OUTPUT + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(150 + 10 * i, 40)), module, Vpc40Module::DEVICE_KNOB_1_OUTPUT + i));
        }
        for(int i = 0; i < C_KNOB_NUM / 2; i++) {
            addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(150 + 10 * i, 20)), module, Vpc40Module::TRACK_KNOB_5_OUTPUT + i));
            addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(150 + 10 * i, 50)), module, Vpc40Module::DEVICE_KNOB_5_OUTPUT + i));
        }
        for(int i = 0; i < CHAN_NUM; i++) {
            addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(10 + 10 * i, 80)), module, Vpc40Module::TRACK_LEVEL_1_OUTPUT + i));
        }
        for(int i = 0; i < CHAN_NUM; i++) {
            addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(10 + 10 * i, 60)), module, Vpc40Module::LED_OUTPUT_1 + i));
        }
        addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(110, 80)), module, Vpc40Module::MASTER_LEVEL_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(130, 80)), module, Vpc40Module::X_FADER_OUTPUT));
        addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(150, 80)), module, Vpc40Module::CUE_OUTPUT));
    }
};

Model* modelVpc40 = createModel<Vpc40Module,Vpc40Widget>("Vpc40Module");
