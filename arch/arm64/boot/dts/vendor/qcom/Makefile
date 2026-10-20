ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_KONA) += \
                kona-cdp-overlay.dtbo \
                kona-cdp-lcd-overlay.dtbo \
                kona-cdp-lcd-tron-overlay.dtbo \
                kona-mtp-overlay.dtbo \
                kona-mtp-ws-overlay.dtbo \
		kona-sa-mtp-overlay.dtbo \
                kona-xr-overlay.dtbo \
                kona-rumi-overlay.dtbo \
                kona-qrd-overlay.dtbo \
                kona-xrfusion-overlay.dtbo \
                kona-xrfusion-ult-overlay.dtbo \
                kona-arglass-overlay.dtbo \
                kona-hdk-overlay.dtbo

kona-cdp-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-cdp-lcd-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-cdp-lcd-tron-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-mtp-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-mtp-ws-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-sa-mtp-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-xr-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-rumi-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-qrd-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-xrfusion-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-xrfusion-ult-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-arglass-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
kona-hdk-overlay.dtbo-base := kona.dtb kona-v2.dtb kona-v2.1.dtb
else
dtb-$(CONFIG_ARCH_KONA) += kona-rumi.dtb \
        kona-mtp.dtb \
        kona-mtp-ws.dtb \
        kona-mtp-sa.dtb \
        kona-xr.dtb \
        kona-xrfusion.dtb \
        kona-xrfusion-ult.dtb \
        kona-arglass.dtb \
        kona-cdp.dtb \
        kona-cdp-lcd.dtb \
        kona-cdp-lcd-tron.dtb \
        kona-qrd.dtb \
        kona-v2-rumi.dtb \
        kona-v2-mtp.dtb \
        kona-v2-mtp-ws.dtb \
        kona-v2-mtp-sa.dtb \
        kona-v2-cdp.dtb \
        kona-v2-qrd.dtb \
        kona-v2-xrfusion.dtb \
        kona-v2-xrfusion-ult.dtb \
        kona-v2-arglass.dtb \
        kona-hdk.dtb \
        kona-v2.1-mtp.dtb \
        kona-v2.1-mtp-ws.dtb \
        kona-v2.1-mtp-sa.dtb \
        kona-v2.1-cdp.dtb \
        kona-v2.1-qrd.dtb \
        kona-v2.1-hdk.dtb \
        kona-v2.1-xrfusion.dtb \
        kona-v2.1-xrfusion-ult.dtb \
        kona-v2.1-arglass.dtb \
        qrb5165-iot-rb5.dtb \
        qrb5165n-iot-rb5.dtb \
        kona-v2.1-iot-rb5.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LITO) += lito-rumi-overlay.dtbo \
                lito-mtp-overlay.dtbo \
                lito-v2-mtp-overlay.dtbo \
                lito-cdp-overlay.dtbo \
                lito-v2-cdp-overlay.dtbo \
                lito-atp-overlay.dtbo \
                lito-v2-atp-overlay.dtbo \
                lito-v2-atp-lcd-overlay.dtbo \
                lito-qrd-overlay.dtbo \
                lito-v2-qrd-overlay.dtbo \
                litomagnus-mtp-overlay.dtbo \
                litomagnus-cdp-overlay.dtbo \
                orchid-mtp-overlay.dtbo \
                orchid-cdp-overlay.dtbo

lito-rumi-overlay.dtbo-base := lito.dtb lito-v2.dtb
lito-mtp-overlay.dtbo-base := lito.dtb
lito-v2-mtp-overlay.dtbo-base := lito-v2.dtb
lito-cdp-overlay.dtbo-base := lito.dtb
lito-v2-cdp-overlay.dtbo-base := lito-v2.dtb
lito-atp-overlay.dtbo-base := lito.dtb
lito-v2-atp-overlay.dtbo-base := lito-v2.dtb
lito-v2-atp-lcd-overlay.dtbo-base := lito-v2.dtb
lito-qrd-overlay.dtbo-base := lito.dtb
lito-v2-qrd-overlay.dtbo-base := lito-v2.dtb
litomagnus-mtp-overlay.dtbo-base := litomagnus.dtb
litomagnus-cdp-overlay.dtbo-base := litomagnus.dtb
orchid-mtp-overlay.dtbo-base := orchid.dtb
orchid-cdp-overlay.dtbo-base := orchid.dtb
else
dtb-$(CONFIG_ARCH_LITO) += lito-rumi.dtb \
                lito-mtp.dtb \
                lito-cdp.dtb \
                lito-atp.dtb \
                lito-qrd.dtb \
                lito-v2-mtp.dtb \
                lito-v2-cdp.dtb \
                lito-v2-atp.dtb \
                lito-v2-atp-lcd.dtb \
                lito-v2-qrd.dtb \
                litomagnus-mtp.dtb \
                litomagnus-cdp.dtb \
                orchid-mtp.dtb \
                orchid-cdp.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_LAGOON) += \
		lagoon-rumi-overlay.dtbo \
		lagoon-mtp-overlay.dtbo \
		lagoon-mtp-usbc-overlay.dtbo \
		lagoon-cdp-overlay.dtbo \
		lagoon-atp-overlay.dtbo \
		lagoon-qrd-overlay.dtbo

lagoon-rumi-overlay.dtbo-base := lagoon.dtb
lagoon-mtp-overlay.dtbo-base := lagoon.dtb
lagoon-mtp-usbc-overlay.dtbo-base := lagoon.dtb
lagoon-cdp-overlay.dtbo-base := lagoon.dtb
lagoon-atp-overlay.dtbo-base := lagoon.dtb
lagoon-qrd-overlay.dtbo-base := lagoon.dtb
else
dtb-$(CONFIG_ARCH_LAGOON) += \
		lagoon-rumi.dtb \
		lagoon-mtp.dtb \
		lagoon-mtp-usbc.dtb \
		lagoon-cdp.dtb \
		lagoon-atp.dtb \
		lagoon-qrd.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_BENGAL) += \
                bengal-rumi-overlay.dtbo \
                bengal-qrd-overlay.dtbo \
                bengal-idp-overlay.dtbo \
                bengal-idp-nopmi-overlay.dtbo \
                bengal-idp-usbc-overlay.dtbo \
                bengalp-idp-overlay.dtbo \
                bengal-idp-1gb-overlay.dtbo \
                bengal-idp-2gb-overlay.dtbo \
                bengal-idp-usbc-1gb-overlay.dtbo \
                bengal-idp-usbc-2gb-overlay.dtbo \
                bengal-iot-idp-overlay.dtbo \
                bengalp-iot-idp-overlay.dtbo \
                bengal-iot-idp-2gb-overlay.dtbo \
                bengal-iot-idp-usbc-2gb-overlay.dtbo \
                bengal-iot-idp-usbc-overlay.dtbo \
                bengalp-iot-idp-2gb-overlay.dtbo \
                bengalp-iot-idp-usbc-2gb-overlay.dtbo \
                bengalp-iot-idp-usbc-overlay.dtbo

bengal-rumi-overlay.dtbo-base := bengal.dtb
bengal-qrd-overlay.dtbo-base := bengal.dtb
bengal-idp-overlay.dtbo-base := bengal.dtb
bengal-idp-nopmi-overlay.dtbo-base := bengal.dtb
bengal-idp-usbc-overlay.dtbo-base := bengal.dtb
bengalp-idp-overlay.dtbo-base := bengalp.dtb
bengal-idp-1gb-overlay.dtbo-base := bengal-1gb.dtb
bengal-idp-2gb-overlay.dtbo-base := bengal-2gb.dtb
bengal-idp-usbc-1gb-overlay.dtbo-base := bengal-1gb.dtb
bengal-idp-usbc-2gb-overlay.dtbo-base := bengal-2gb.dtb
bengal-iot-idp-overlay.dtbo-base := bengal-iot.dtb
bengalp-iot-idp-overlay.dtbo-base := bengalp-iot.dtb
bengal-iot-idp-2gb-overlay.dtbo-base := bengal-iot-2gb.dtb
bengal-iot-idp-usbc-2gb-overlay.dtbo-base := bengal-iot-2gb.dtb
bengal-iot-idp-usbc-overlay.dtbo-base := bengal-iot.dtb
bengalp-iot-idp-2gb-overlay.dtbo-base := bengalp-iot-2gb.dtb
bengalp-iot-idp-usbc-2gb-overlay.dtbo-base := bengalp-iot-2gb.dtb
bengalp-iot-idp-usbc-overlay.dtbo-base := bengalp-iot.dtb
else
dtb-$(CONFIG_ARCH_BENGAL) += bengal-rumi.dtb \
                bengal-qrd.dtb \
		bengal-idp.dtb \
		bengal-idp-nopmi.dtb \
		bengal-idp-usbc.dtb \
		bengalp-idp.dtb \
		bengal-idp-1gb.dtb \
		bengal-idp-2gb.dtb \
		bengal-idp-usbc-1gb.dtb \
		bengal-idp-usbc-2gb.dtb \
		bengal-iot-idp.dtb \
		bengalp-iot-idp.dtb \
		bengal-iot-idp-2gb.dtb \
		bengal-iot-idp-usbc-2gb.dtb \
		bengal-iot-idp-usbc.dtb \
		bengalp-iot-idp-2gb.dtb \
		bengalp-iot-idp-usbc-2gb.dtb \
		bengalp-iot-idp-usbc.dtb \
		bengalp-iot-idp-2gb.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_SCUBA) += \
		scuba-rumi-overlay.dtbo \
		scuba-idp-overlay.dtbo \
		scuba-idp-usbc-overlay.dtbo \
		scuba-qrd-eldo-overlay.dtbo \
		scuba-qrd-non-eldo-overlay.dtbo \
		scuba-iot-idp-overlay.dtbo \
		scuba-iot-idp-usbc-overlay.dtbo \
		scuba-iot-qrd-eldo-overlay.dtbo \
		scuba-iot-qrd-non-eldo-overlay.dtbo

scuba-rumi-overlay.dtbo-base := scuba.dtb scubap.dtb scuba-2gb.dtb
scuba-idp-overlay.dtbo-base := scuba.dtb scubap.dtb scuba-2gb.dtb
scuba-qrd-eldo-overlay.dtbo-base := scuba.dtb scubap.dtb scuba-2gb.dtb
scuba-qrd-non-eldo-overlay.dtbo-base := scuba.dtb scubap.dtb scuba-2gb.dtb
scuba-idp-usbc-overlay.dtbo-base := scuba.dtb scubap.dtb scuba-2gb.dtb
scuba-iot-idp-overlay.dtbo-base := scuba-iot.dtb scuba-iot-2gb.dtb scubap-iot.dtb scubap-iot-idp.dtb scubap-iot-idp-2gb.dtb
scuba-iot-qrd-eldo-overlay.dtbo-base := scuba-iot.dtb scubap-iot.dtb scuba-iot-2gb.dtb
scuba-iot-qrd-non-eldo-overlay.dtbo-base := scuba-iot.dtb scubap-iot.dtb scuba-iot-2gb.dtb
scuba-iot-idp-usbc-overlay.dtbo-base := scuba-iot.dtb scubap-iot.dtb scuba-iot-2gb.dtb
else
dtb-$(CONFIG_ARCH_SCUBA) += scuba-rumi.dtb \
	scuba-idp.dtb \
	scuba-idp-usbc.dtb \
	scuba-qrd-eldo.dtb \
	scuba-qrd-non-eldo.dtb \
	scubap-idp.dtb \
	scubap-idp-2gb.dtb \
	scuba-idp-2gb.dtb \
	scuba-idp-usbc-2gb.dtb \
	scuba-iot-idp.dtb \
	scuba-iot-idp-usbc.dtb \
	scubap-iot-idp.dtb \
	scuba-iot-qrd-eldo.dtb \
	scuba-iot-qrd-non-eldo.dtb \
	scubap-iot-idp-2gb.dtb \
	scuba-iot-idp-2gb.dtb \
	scuba-iot-idp-usbc-2gb.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_KHAJE) += \
		khaje-idp-overlay.dtbo \
		khaje-qrd-overlay.dtbo \
		khaje-qrd-hvdcp3p5-overlay.dtbo \
		khaje-qrd-nowcd9375-overlay.dtbo \
		khaje-idp-nopmi-overlay.dtbo \
		khaje-idp-usbc-overlay.dtbo \
		khaje-idp-pm8010-overlay.dtbo \
		khaje-qrd-nopmi-overlay.dtbo \
		khaje-idps-display-90hz-overlay.dtbo \
		khaje-atp-overlay.dtbo

khaje-idp-overlay.dtbo-base := khaje.dtb
khaje-qrd-overlay.dtbo-base := khaje.dtb
khaje-qrd-hvdcp3p5-overlay.dtbo-base := khaje.dtb
khaje-qrd-nowcd9375-overlay.dtbo-base := khaje.dtb
khaje-idp-nopmi-overlay.dtbo-base := khaje.dtb
khaje-idp-usbc-overlay.dtbo-base := khaje.dtb
khaje-idp-pm8010-overlay.dtbo-base := khaje.dtb
khaje-qrd-nopmi-overlay.dtbo-base := khaje.dtb
khaje-idps-display-90hz-overlay.dtbo-base := khaje.dtb
khaje-atp-overlay.dtbo-base := khaje.dtb
else
dtb-$(CONFIG_ARCH_KHAJE) += khaje-idp.dtb \
		khaje-qrd.dtb \
		khaje-qrd-hvdcp3p5.dtb \
		khaje-qrd-nowcd9375.dtb \
		khaje-idp-nopmi.dtb \
		khaje-idp-usbc.dtb \
		khaje-idp-pm8010.dtb \
		khaje-qrd-nopmi.dtb \
		khaje-idps-display-90hz.dtb \
		khaje-atp.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
        dtbo-$(CONFIG_ARCH_SDM660) += \
                sdm660-mtp-external-codec-overlay.dtbo \
                sdm660-mtp-internal-codec-overlay.dtbo \
                sdm660-cdp-external-codec-overlay.dtbo \
                sdm660-cdp-internal-codec-overlay.dtbo \
                sdm660-qrd-external-codec-overlay.dtbo \
                sdm660-rcm-external-codec-overlay.dtbo \
                sdm660-rcm-internal-codec-overlay.dtbo \
                sda660-mtp-external-codec-overlay.dtbo \
                sda660-cdp-external-codec-overlay.dtbo \
                sda660-rcm-external-codec-overlay.dtbo

sdm660-mtp-external-codec-overlay.dtbo-base := sdm660-pm660l.dtb

sdm660-mtp-internal-codec-overlay.dtbo-base := sdm660-pm660l.dtb

sdm660-cdp-external-codec-overlay.dtbo-base := sdm660-pm660l.dtb

sdm660-cdp-internal-codec-overlay.dtbo-base := sdm660-pm660l.dtb

sdm660-qrd-external-codec-overlay.dtbo-base := sdm660-pm660l.dtb

sdm660-rcm-external-codec-overlay.dtbo-base := sdm660-pm660l.dtb

sdm660-rcm-internal-codec-overlay.dtbo-base := sdm660-pm660l.dtb

sda660-mtp-external-codec-overlay.dtbo-base := sda660-pm660l.dtb

sda660-cdp-external-codec-overlay.dtbo-base := sda660-pm660l.dtb

sda660-rcm-external-codec-overlay.dtbo-base := sda660-pm660l.dtb
else
dtb-$(CONFIG_ARCH_SDM660) += sdm660-sim.dtb \
sdm660-internal-codec-cdp.dtb \
        sdm660-internal-codec-mtp.dtb \
        sdm660-internal-codec-rcm.dtb \
        sdm660-cdp.dtb \
        sdm660-mtp.dtb \
        sdm660-qrd.dtb \
        sdm660-rcm.dtb \
        sdm660-pm660a-cdp.dtb \
        sdm660-pm660a-mtp.dtb \
        sdm660-pm660a-qrd.dtb \
        sdm660-pm660a-rcm.dtb \
        sdm660-internal-codec-pm660a-cdp.dtb \
        sdm660-internal-codec-pm660a-mtp.dtb \
        sdm660-internal-codec-pm660a-rcm.dtb \
        sdm660-pm660a-sim.dtb \
        sda660-cdp.dtb \
        sda660-mtp.dtb \
        sda660-rcm.dtb \
        sda660-pm660a-cdp.dtb \
        sda660-pm660a-mtp.dtb \
        sda660-pm660a-rcm.dtb \
        sda660-pm660a-qrd-hdk.dtb \
        sdm660-headset-jacktype-no-cdp.dtb \
        sdm660-headset-jacktype-no-rcm.dtb \
        sdm660-pm660a-headset-jacktype-no-cdp.dtb \
        sdm660-pm660a-headset-jacktype-no-rcm.dtb \
        sdm660-usbc-audio-mtp.dtb \
        sdm660-usbc-audio-rcm.dtb \
        sdm636-cdp.dtb \
        sdm636-mtp.dtb \
        sdm636-qrd.dtb \
        sdm636-rcm.dtb \
        sdm636-headset-jacktype-no-cdp.dtb \
        sdm636-headset-jacktype-no-rcm.dtb \
        sdm636-internal-codec-cdp.dtb \
        sdm636-internal-codec-mtp.dtb \
        sdm636-internal-codec-pm660a-cdp.dtb \
        sdm636-internal-codec-pm660a-mtp.dtb \
        sdm636-internal-codec-pm660a-rcm.dtb \
        sdm636-internal-codec-rcm.dtb \
        sdm636-pm660a-headset-jacktype-no-cdp.dtb \
        sdm636-pm660a-headset-jacktype-no-rcm.dtb \
        sdm636-pm660a-cdp.dtb \
        sdm636-pm660a-mtp.dtb \
        sdm636-pm660a-qrd.dtb \
        sdm636-pm660a-rcm.dtb \
        sdm636-usbc-audio-mtp.dtb \
        sdm636-usbc-audio-rcm.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)

dtbo-$(CONFIG_ARCH_SDM439) += sdm439-mtp-overlay.dtbo \
        sdm439-cdp-overlay.dtbo \
        sdm439-qrd-overlay.dtbo \
        sdm439-external-codec-mtp-overlay.dtbo \
        sdm439-rcm-overlay.dtbo

dtbo-$(CONFIG_ARCH_SDM429) += sdm429-mtp-overlay.dtbo \
	sdm429-cdp-overlay.dtbo \
	sdm429-qrd-overlay.dtbo

dtbo-$(CONFIG_ARCH_QM215) +=qm215-qrd-overlay.dtbo \
	qcm2150-qrd-overlay.dtbo \
	qm215-qrd-smb1360-overlay.dtbo

sdm439-mtp-overlay.dtbo-base := sdm439.dtb \
	sda439.dtb \
	msm8937-interposer-sdm439.dtb

sdm439-cdp-overlay.dtbo-base := sdm439.dtb \
        sda439.dtb \
        msm8937-interposer-sdm439.dtb

sdm439-qrd-overlay.dtbo-base := sdm439.dtb \
        msm8937-interposer-sdm439.dtb

sdm439-external-codec-mtp-overlay.dtbo-base := sdm439.dtb
sdm439-rcm-overlay.dtbo-base := sdm439.dtb


sdm429-mtp-overlay.dtbo-base := sdm429.dtb \
	sda429.dtb \
	msm8937-interposer-sdm429.dtb

sdm429-cdp-overlay.dtbo-base := sdm429.dtb \
	sda429.dtb \
	msm8937-interposer-sdm429.dtb

sdm429-qrd-overlay.dtbo-base := sdm429.dtb \
	msm8937-interposer-sdm429.dtb

qm215-qrd-overlay.dtbo-base := qm215.dtb
qcm2150-qrd-overlay.dtbo-base := qcm2150.dtb
qm215-qrd-smb1360-overlay.dtbo-base := qm215.dtb
else
dtb-$(CONFIG_ARCH_SDM439) += sdm439-mtp.dtb \
	sdm439-cdp.dtb \
	sdm439-qrd.dtb \
	sda439-mtp.dtb \
	sda439-cdp.dtb \
	sdm439-external-codec-mtp.dtb \
	sdm439-rcm.dtb
dtb-$(CONFIG_ARCH_QM215) += qm215-qrd.dtb \
	qcm2150-qrd.dtb \
	qm215-qrd-smb1360.dtb

dtb-$(CONFIG_ARCH_SDM429) += sdm429-mtp.dtb \
	sdm429-cdp.dtb \
	sdm429-qrd.dtb \
	sda429-mtp.dtb \
	sda429-cdp.dtb
endif

ifeq ($(CONFIG_ARCH_SDM630),y)
dtb-$(CONFIG_ARCH_SDM630) += sdm630-cdp.dtb \
sdm630-mtp.dtb \
        sdm630-rcm.dtb \
        sdm630-headset-jacktype-no-cdp.dtb \
        sdm630-headset-jacktype-no-rcm.dtb \
        sdm630-internal-codec-cdp.dtb \
        sdm630-internal-codec-mtp.dtb \
        sdm630-internal-codec-pm660a-cdp.dtb \
        sdm630-internal-codec-pm660a-mtp.dtb \
        sdm630-internal-codec-pm660a-rcm.dtb \
        sdm630-internal-codec-rcm.dtb \
        sdm630-pm660a-headset-jacktype-no-cdp.dtb \
        sdm630-pm660a-headset-jacktype-no-rcm.dtb \
        sdm630-pm660a-cdp.dtb \
        sdm630-pm660a-mtp.dtb \
        sdm630-pm660a-qrd.dtb \
        sdm630-pm660a-rcm.dtb \
        sdm630-usbc-audio-mtp.dtb \
        sdm630-usbc-audio-rcm.dtb \
        sda630-cdp.dtb \
        sda630-mtp.dtb \
        sda630-rcm.dtb \
        sda630-pm660a-cdp.dtb \
        sda630-pm660a-mtp.dtb \
        sda630-pm660a-rcm.dtb \
        sda630-pm660a-qrd-hdk.dtb
endif

ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
dtbo-$(CONFIG_ARCH_MSM8953) += msm8953-mtp-overlay.dtbo \
	msm8953-cdp-overlay.dtbo \
	msm8953-rcm-overlay.dtbo \
	msm8953-ipc-overlay.dtbo \
	msm8953-qrd-overlay.dtbo \
	msm8953-iot-mtp-overlay.dtbo \
	msm8953-ext-codec-mtp-overlay.dtbo \
	msm8953-ext-codec-rcm-overlay.dtbo \
	msm8953-cdp-1200p-overlay.dtbo

dtbo-$(CONFIG_ARCH_SDM450) += msm8953-mtp-overlay.dtbo \
	msm8953-cdp-overlay.dtbo \
	msm8953-rcm-overlay.dtbo \
	msm8953-qrd-overlay.dtbo \
	msm8953-iot-mtp-overlay.dtbo \
	sdm450-cdp-s2-overlay.dtbo \
	sdm450-mtp-s3-overlay.dtbo \
	sdm450-qrd-sku4-overlay.dtbo

dtbo-$(CONFIG_ARCH_SDM632) += sdm632-rumi-overlay.dtbo \
	sdm450-cdp-s2-overlay.dtbo \
	sdm450-mtp-s3-overlay.dtbo \
	sdm632-ext-codec-cdp-s3-overlay.dtbo \
	sdm632-ext-codec-mtp-s4-overlay.dtbo \
	sdm632-rcm-overlay.dtbo \
	sdm450-qrd-sku4-overlay.dtbo \
	sdm632-qrd-overlay.dtbo

msm8953-mtp-overlay.dtbo-base := sdm450.dtb \
	msm8953.dtb \
	apq8053.dtb \
	sda450.dtb \
	msm8953-pmi8940.dtb \
	msm8953-pmi8937.dtb \
	sdm450-pmi8940.dtb \
	sdm450-pmi8937.dtb
msm8953-cdp-overlay.dtbo-base := sdm450.dtb \
	msm8953.dtb \
	apq8053.dtb \
	sda450.dtb \
	msm8953-pmi8940.dtb \
	msm8953-pmi8937.dtb
msm8953-rcm-overlay.dtbo-base := sdm450.dtb \
	msm8953.dtb \
	apq8053.dtb
msm8953-ipc-overlay.dtbo-base := msm8953.dtb \
	apq8053.dtb
msm8953-qrd-overlay.dtbo-base := sdm450.dtb \
	msm8953.dtb
msm8953-iot-mtp-overlay.dtbo-base := sdm450.dtb \
	msm8953.dtb \
	apq8053.dtb
msm8953-ext-codec-mtp-overlay.dtbo-base := msm8953.dtb \
	apq8053.dtb \
	msm8953-pmi8940.dtb \
	msm8953-pmi8937.dtb
msm8953-ext-codec-rcm-overlay.dtbo-base := msm8953.dtb \
	apq8053.dtb
msm8953-cdp-1200p-overlay.dtbo-base := msm8953.dtb
sdm450-cdp-s2-overlay.dtbo-base := sdm450-pmi632.dtb \
	sdm632.dtb \
	sdm632-pm8004.dtb \
	msm8953-pmi632.dtb \
	sda450-pmi632.dtb
sdm450-mtp-s3-overlay.dtbo-base := sdm450-pmi632.dtb \
	sdm632.dtb \
	sdm632-pm8004.dtb \
	sda450-pmi632.dtb
sdm450-qrd-sku4-overlay.dtbo-base := sdm450-pmi632.dtb \
	sdm632.dtb \
	sdm632-pm8004.dtb
sdm632-rumi-overlay.dtbo-base := sdm632.dtb
sdm632-ext-codec-cdp-s3-overlay.dtbo-base := sdm632.dtb \
	sdm632-pm8004.dtb
sdm632-ext-codec-mtp-s4-overlay.dtbo-base := sdm632.dtb \
	sdm632-pm8004.dtb
sdm632-rcm-overlay.dtbo-base := sdm632.dtb \
	sdm632-pm8004.dtb
sdm632-qrd-overlay.dtbo-base := sdm632.dtb \
	sdm632-pm8004.dtb

else
ifeq ($(CONFIG_MACH_XIAOMI_MSM8953),y)
# ⚠️ 本仓库面向 OPPO，不打包任何小米 DTB。
#
# 原因（实测踩到）：这几个 DTB 会被 cat 进 Image.gz-dtb，ABL 是按
# msm-id/board-id 自前向后扫描取**第一个**匹配。这几个小米 DTB 恰好排在
# 前面，其中 ysl-titanium.dtb 的 board-id = <0x08 0x00>（只有两格，
# 等于 subtype 通配），会先于 OPPO 的 msm8953-mtp_16027.dtb
# (<0x08 0x00 0x3e9b>) 命中，于是 R9s 拿到的是红米 S2 的设备树 →
# 内核跑在与硬件不符的 DTS 上，早期就崩，表现为重启进 fastboot。
#
# 注意：CONFIG_MACH_XIAOMI_MSM8953 这个**符号本身仍然必须为 y** ——
# 4.19 基线里 mdss_dsi.c / msm_flash.c / msm8952.c / qpnp-fg.c 等
# 有未加保护的 xiaomi_msm8953_mach_get() 调用（详见 README-4.19-port.md），
# 关掉它反而编不过。所以这里保留符号、只去掉 DTB 条目。
# （对比：能正常开机的 lineage-22.1 R9s defconfig 里小米机型数为 0，
#   其镜像只含 16017/16027 两个 DTB。）
dtb-$(CONFIG_MACH_XIAOMI_ROSY) +=
dtb-$(CONFIG_MACH_XIAOMI_VINCE) +=
dtb-$(CONFIG_MACH_XIAOMI_YSL) +=
dtb-$(CONFIG_MACH_XIAOMI_MIDO) +=
else
dtb-$(CONFIG_ARCH_MSM8953) += msm8953-cdp.dtb \
	msm8953-mtp.dtb \
	msm8953-ext-codec-mtp.dtb \
	msm8953-qrd-sku3.dtb \
	msm8953-rcm.dtb \
	apq8053-rcm.dtb \
	msm8953-ext-codec-rcm.dtb \
	apq8053-cdp.dtb \
	apq8053-ipc.dtb \
	msm8953-ipc.dtb \
	apq8053-mtp.dtb \
	apq8053-ext-audio-mtp.dtb \
	apq8053-ext-codec-rcm.dtb \
	msm8953-cdp-1200p.dtb \
	msm8953-iot-mtp.dtb \
	apq8053-iot-mtp.dtb \
	apq8053-lite-dragon-v1.0.dtb \
	apq8053-lite-dragon-v2.0.dtb \
	apq8053-lite-dragon-v2.1.dtb \
	apq8053-lite-dragon-v2.2.dtb \
	apq8053-lite-dragon-v2.2.1.dtb \
	apq8053-lite-dragon-v2.3.dtb \
	apq8053-lite-dragon-v2.4.dtb \
	apq8053-lat-concam-dev.dtb \
	apq8053-lat-concam-proto.dtb \
	msm8953-pmi8940-cdp.dtb \
	msm8953-pmi8940-mtp.dtb \
	msm8953-pmi8937-cdp.dtb \
	msm8953-pmi8937-mtp.dtb \
	msm8953-pmi8940-ext-codec-mtp.dtb \
	msm8953-pmi8937-ext-codec-mtp.dtb \
	msm8953-pmi632-cdp-s2.dtb \
	apq8053-batcam.dtb

dtb-$(CONFIG_ARCH_SDM450) += sdm450-rcm.dtb \
	sdm450-cdp.dtb \
	sdm450-mtp.dtb \
	sda450-cdp.dtb \
	sda450-mtp.dtb \
	sdm450-qrd.dtb \
	sdm450-pmi8940-mtp.dtb \
	sdm450-pmi8937-mtp.dtb \
	sdm450-iot-mtp.dtb \
	sdm450-qrd-sku4.dtb \
	sdm450-pmi632-cdp-s2.dtb \
	sdm450-pmi632-mtp-s3.dtb \
	sda450-pmi632-cdp-s2.dtb \
	sda450-pmi632-mtp-s3.dtb

dtb-$(CONFIG_ARCH_SDM632) += sdm632-rumi.dtb \
	sdm632-cdp-s2.dtb \
	sdm632-ext-codec-cdp-s3.dtb \
	sdm632-mtp-s3.dtb \
	sdm632-ext-codec-mtp-s4.dtb \
	sdm632-rcm.dtb \
	sdm632-qrd-sku4.dtb \
	sdm632-qrd.dtb \
	sdm632-pm8004-cdp-s2.dtb \
	sdm632-pm8004-ext-codec-cdp-s3.dtb \
	sdm632-pm8004-mtp-s3.dtb \
	sdm632-pm8004-ext-codec-mtp-s4.dtb \
	sdm632-pm8004-rcm.dtb \
	sdm632-pm8004-qrd-sku4.dtb \
	sdm632-pm8004-qrd.dtb

endif
endif

# OPPO R9s / R9sk (MSM8953) —— 板级 DTB
# 放在所有 ifeq 之外：dtb-$(CONFIG_MACH_16017) 本身带条件，位置无关。
# 这两个 DTB 依赖 oppo-msm8953/ 下的板级 dtsi 覆盖层。
dtb-$(CONFIG_MACH_16017) += msm8953-mtp_16017.dtb
dtb-$(CONFIG_MACH_16027) += msm8953-mtp_16027.dtb

# OPPO A57 (MSM8940 / PMI8950) —— 板级 DTB
# MSM8940 与 MSM8937 同芯片不同 SKU，板级 DTS 在 oppo-msm8937/ 下：
#   msm8940.dtsi                 #include ../msm8937.dtsi + 覆盖 msm-id/USB/GPU
#   msm8940-pmi8950-mtp.dtsi     再叠加 pmi8950 与 mtp 板级配置
# board-id = <8 0 16061>，与 R9s 的 <8 0 16017/16027> 互不冲突。
dtb-$(CONFIG_MACH_16061) += msm8940-pmi8950-mtp_16061.dtb

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files    := *.dtb *.dtbo
