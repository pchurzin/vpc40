#include "plugin.hpp"
#include "VpcMidiDisplay.hpp"
#include "vpc_protocol.hpp"

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
        NUM_OUTPUTS
    };
    enum LightIds {
        RESET_LIGHT,
        TEST_LIGHT,
        NUM_LIGHTS
    };

    rack::midi::InputQueue midiInput;
    rack::midi::Output midiOutput;
    ptone::IoPort ioPort;
    dsp::BooleanTrigger resetButtonTrigger;
    dsp::BooleanTrigger testButtonTrigger;
    dsp::Timer rateLimitTimer;
    float rateLimitPeriod = 1 / 200.f;
    uint8_t sysExDeviceId = -1;

    float device1 = 0.f;
    uint8_t bank = 0;
    bool bankChanged = false;

    // track knob values 
    float trackKnobsVoltage[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    uint8_t trackKnobsMidi[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    bool trackKnobUpdates[PORT_MAX_CHANNELS * C_KNOB_NUM] = {true};
    // device knob values 
    float deviceKnobVoltage[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    uint8_t deviceKnobsMidi[PORT_MAX_CHANNELS * C_KNOB_NUM] = {0};
    bool deviceKnobUpdates[PORT_MAX_CHANNELS * C_KNOB_NUM] = {true};

    Vpc40Module() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configButton(RESET_PARAM, "Reset");
        configButton(TEST_PARAM, "Test");
        for (int i = 0; i < C_KNOB_NUM; i++) {
            configOutput(DEVICE_KNOB_1_OUTPUT + i, string::f("Device %d", i + 1));
            outputs[DEVICE_KNOB_1_OUTPUT + i].setChannels(PORT_MAX_CHANNELS - 1);
            configOutput(TRACK_KNOB_1_OUTPUT + i, string::f("Track %d", i + 1));
            outputs[TRACK_KNOB_1_OUTPUT + i].setChannels(PORT_MAX_CHANNELS - 1);
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
        rack::midi::Message inboundMidi;
        while (midiInput.tryPop(&inboundMidi, args.frame)) {
            //DEBUG("Channel: %d, Status: %d, Note/CC: %d, Value: %d", inboundMidi.getChannel(), inboundMidi.getStatus(), inboundMidi.getNote(), inboundMidi.getValue());
            if (inboundMidi.bytes[0] == 0xF0 && 
                    inboundMidi.bytes[3] == 0x06 &&
                    inboundMidi.bytes[4] == 0x02) {
                processInquireResponse(inboundMidi);
                introduce();
            }
            
            if (inboundMidi.getStatus() == STATUS_NOTE_ON) {
                // send note on back
                setLedOn(args.frame, inboundMidi.getChannel(), inboundMidi.getNote());

                if (inboundMidi.getNote() == BTN_RIGHT) {
                    bank = bank + 1;
                    if (bank > PORT_MAX_CHANNELS - 1) bank = 0;
                    bankChanged = true;
                } else if (inboundMidi.getNote() == BTN_LEFT) {
                    if (bank == 0) {
                        bank = PORT_MAX_CHANNELS - 1;
                    } else {
                        bank = bank - 1;
                    }
                    bankChanged = true;
                }
            }
            else if (inboundMidi.getStatus() == STATUS_NOTE_OFF) {
                setLedOff(args.frame, inboundMidi.getChannel(), inboundMidi.getNote());
            }
            else if (inboundMidi.getStatus() == STATUS_CC) {
                uint8_t cc = inboundMidi.getNote();
                if (isDeviceKnob(cc)) {
                    int knob = cc - C_DEVICE_KNOB_1;
                    int ki = knobIndex(knob, bank);
                    uint8_t oldMidiValue = deviceKnobsMidi[ki];
                    uint8_t newMidiValue = inboundMidi.getValue();
                    if(oldMidiValue != newMidiValue) {
                        deviceKnobVoltage[ki] = 10.f * clamp(inboundMidi.getValue() / 127.f, 0.f, 1.f); 
                        deviceKnobsMidi[ki] = newMidiValue;
                        deviceKnobUpdates[ki] = true;
                    }
                }
                else if (isTrackKnob(cc)) {
                    int knob = cc - C_TRACK_KNOB_1;
                    int ki = knobIndex(knob, bank);
                    uint8_t oldMidiValue = deviceKnobsMidi[ki];
                    uint8_t newMidiValue = inboundMidi.getValue();
                    if(oldMidiValue != newMidiValue) {
                        trackKnobsVoltage[ki] = 10.f * clamp(inboundMidi.getValue() / 127.f, 0.f, 1.f);
                        trackKnobsMidi[ki] = inboundMidi.getValue();
                        trackKnobUpdates[ki] = true;
                    }
                }
            }
        }

        if (rateLimitTriggered) {
            for (int k = 0; k < C_KNOB_NUM; k++) {
                int ki = knobIndex(k, bank);
                if(bankChanged || trackKnobUpdates[ki]) {
                    trackKnobUpdates[ki] = false;
                    setCc(args.frame, 0, C_TRACK_KNOB_1 + k, trackKnobsMidi[ki]);
                }
                if(bankChanged || deviceKnobUpdates[ki]) {
                    deviceKnobUpdates[ki] = false;
                    setCc(args.frame, 0, C_DEVICE_KNOB_1 + k, deviceKnobsMidi[ki]);
                }
            }
            bankChanged = false;
        }

        for (uint8_t c = 0; c < PORT_MAX_CHANNELS; c++) {
            for(int k = 0; k < C_KNOB_NUM; k++) {
                // update track knob outputs
                int ki = knobIndex(k, c);
                if (outputs[TRACK_KNOB_1_OUTPUT + k].isConnected()) {
                    outputs[TRACK_KNOB_1_OUTPUT + k].channels = PORT_MAX_CHANNELS;
                    outputs[TRACK_KNOB_1_OUTPUT + k].setVoltage(trackKnobsVoltage[ki], c);
                }
                // update device knob outputs
                if (outputs[DEVICE_KNOB_1_OUTPUT + k].isConnected()) {
                    outputs[DEVICE_KNOB_1_OUTPUT + k].channels = PORT_MAX_CHANNELS;
                    outputs[DEVICE_KNOB_1_OUTPUT + k].setVoltage(deviceKnobVoltage[ki], c);
                }
            }
        }
    }

    int knobIndex(uint8_t knob, uint8_t bank) {
        return knob * PORT_MAX_CHANNELS + bank;
    }

    bool isDeviceKnob(uint8_t cc) {
        return cc >= C_DEVICE_KNOB_1 && cc <= C_DEVICE_KNOB_8;
    }

    bool isTrackKnob(uint8_t cc) {
        return cc >= C_DEVICE_KNOB_1 && cc <= C_TRACK_KNOB_8;
    }

    void setCc(int64_t frame, uint8_t midiChannel, uint8_t cc, uint8_t value) {
        rack::midi::Message msg;
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
        rack::midi::Message msg;
        msg.setFrame(frame);
        msg.setChannel(midiChannel);
        msg.setNote(note);
        msg.setStatus(STATUS_NOTE_OFF);
        msg.setValue(0);
        midiOutput.sendMessage(msg);
    }
    void setLedOn(int64_t frame, uint8_t midiChannel, uint8_t note, uint8_t ledValue) {
        rack::midi::Message msg;
        msg.setFrame(frame);
        msg.setChannel(midiChannel);
        msg.setNote(note);
        msg.setStatus(STATUS_NOTE_ON);
        msg.setValue(ledValue);
        midiOutput.sendMessage(msg);
    }
    void inquireDevice() {
        rack::midi::Message msg;
        msg.setSize(6);
        msg.bytes[0] = 0xF0;
        msg.bytes[1] = 0x7E;
        msg.bytes[2] = 0x00;
        msg.bytes[3] = 0x06;
        msg.bytes[4] = 0x01;
        msg.bytes[5] = 0xF7;
        midiOutput.sendMessage(msg);
    }

    void processInquireResponse(rack::midi::Message& msg) {
        //todo: fix the midi button
        midiOutput.setChannel(-1);
        sysExDeviceId = msg.bytes[13];
    }

    void introduce() {
        rack::midi::Message msg;
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

    void testMidi(const ProcessArgs& args) {
        rack::midi::Message msg;
        msg.setFrame(args.frame);
        msg.setStatus(0x09);
        msg.setChannel(0);
        msg.setNote(LED_RECORD);
        msg.setValue(LED_ON);
        midiOutput.sendMessage(msg);
    }

    void testLedRingType(const ProcessArgs& args) {
        rack::midi::Message msg;
        msg.setFrame(args.frame);
        msg.setStatus(0x0B);
        msg.setNote(C_DEVICE_KNOB_RING_TYPE_1);
        msg.setValue(RING_TYPE_PAN);
        midiOutput.sendMessage(msg);
    }
    void testLedRing(const ProcessArgs& args) {
        rack::midi::Message msg;
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
    }
};

Model* modelVpc40 = createModel<Vpc40Module,Vpc40Widget>("Vpc40Module");
