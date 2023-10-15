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
        DEVICE_1_OUTPUT,
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
    uint8_t sysExDeviceId = -1;

    float device1 = 0.f;

    Vpc40Module() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configButton(RESET_PARAM, "Reset");
        configButton(TEST_PARAM, "Test");
        configOutput(DEVICE_1_OUTPUT, "Device1");
        ioPort.input = &midiInput;
        ioPort.output = &midiOutput;
    }

    void process(const ProcessArgs &args) override {
        outputs[DEVICE_1_OUTPUT].setVoltage(10.f * clamp(device1, 0.f, 1.f));
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
            DEBUG("Channel: %d, Note: %d, Status: %d", inboundMidi.getChannel(), inboundMidi.getNote(), inboundMidi.getStatus());
            if (inboundMidi.bytes[0] == 0xF0 && 
                    inboundMidi.bytes[3] == 0x06 &&
                    inboundMidi.bytes[4] == 0x02) {
                processInquireResponse(inboundMidi);
                introduce();
            }
            if (inboundMidi.getStatus() == STATUS_NOTE_ON) {
                // send note on back
                setLedOn(args.frame, inboundMidi.getChannel(), inboundMidi.getNote());
            }
            if (inboundMidi.getStatus() == STATUS_NOTE_OFF) {
                setLedOff(args.frame, inboundMidi.getChannel(), inboundMidi.getNote());
            }
            if (inboundMidi.getStatus() == STATUS_CC) {
                setCc(args.frame, inboundMidi.getChannel(), inboundMidi.getNote(), inboundMidi.getValue());
                if(inboundMidi.getNote() == C_DEVICE_KNOB_1) {
                    device1 = inboundMidi.getValue() / 127.f;
                }
            }
        }
    }

    void setCc(int64_t frame, uint8_t channel, uint8_t cc, uint8_t value) {
        rack::midi::Message msg;
        msg.setFrame(frame);
msg.setChannel(channel);
        msg.setNote(cc);
        msg.setStatus(STATUS_CC);
        msg.setValue(value);
        midiOutput.sendMessage(msg);
    }

    void setLedOn(int64_t frame, uint8_t channel, uint8_t note) {
        setLedOn(frame, channel, note, LED_ON);
    }

    void setLedOff(int64_t frame, uint8_t channel, uint8_t note) {
        rack::midi::Message msg;
        msg.setFrame(frame);
        msg.setChannel(channel);
        msg.setNote(note);
        msg.setStatus(STATUS_NOTE_OFF);
        msg.setValue(0);
        midiOutput.sendMessage(msg);
    }
    void setLedOn(int64_t frame, uint8_t channel, uint8_t note, uint8_t ledValue) {
        rack::midi::Message msg;
        msg.setFrame(frame);
        msg.setChannel(channel);
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

        addOutput(createOutputCentered<ThemedPJ301MPort>(mm2px(Vec(120, 70)), module, Vpc40Module::DEVICE_1_OUTPUT));
    }
};

Model* modelVpc40 = createModel<Vpc40Module,Vpc40Widget>("Vpc40Module");
