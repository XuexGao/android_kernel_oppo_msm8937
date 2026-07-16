ccflags-y += -Idrivers/media/platform/msm/camera_v2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
ccflags-y += -Idrivers/media/platform/msm/camera_v2/codecs
ccflags-y += -Idrivers/media/platform/msm/camera_v2/isp
ccflags-y += -Idrivers/media/platform/msm/camera_v2/pproc
ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_vb2
ccflags-y += -Idrivers/media/platform/msm/camera_v2/camera
ccflags-y += -Idrivers/media/platform/msm/camera_v2/jpeg_10
ccflags-y += -Idrivers/media/platform/msm/camera_v2/jpeg_dma
ccflags-y += -Idrivers/media/platform/msm/camera_v2/fd
ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_buf_mgr

# Propagate include paths to all subdirectories
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/io
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/sensor/cci
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/codecs
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/isp
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/pproc
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_vb2
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/camera
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/jpeg_10
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/jpeg_dma
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/fd
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/common
subdir-ccflags-y += -Idrivers/media/platform/msm/camera_v2/msm_buf_mgr

obj-$(CONFIG_MSMB_CAMERA) += common/
obj-$(CONFIG_MSMB_CAMERA) += msm.o
obj-$(CONFIG_MSMB_CAMERA) += camera/
obj-$(CONFIG_MSMB_CAMERA) += msm_vb2/
obj-$(CONFIG_MSMB_CAMERA) += sensor/
obj-$(CONFIG_MSMB_CAMERA) += pproc/
obj-$(CONFIG_MSMB_CAMERA) += isp/
obj-$(CONFIG_MSMB_CAMERA) += ispif/
obj-$(CONFIG_MSMB_JPEG) += jpeg_10/
obj-$(CONFIG_MSM_JPEGDMA) += jpeg_dma/
obj-$(CONFIG_MSMB_CAMERA) += msm_buf_mgr/
obj-$(CONFIG_MSM_FD) += fd/

subdir-ccflags-$(CONFIG_DEBUG_MSM_CAMERA_V2) := -DDEBUG
