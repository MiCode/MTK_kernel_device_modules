#ifndef _AW_WRAPPER_H_
#define _AW_WRAPPER_H_

#include <sound/pcm.h>
#include <linux/workqueue.h>

extern int aw_haptic_open(struct snd_pcm_substream *substream);
extern int aw_haptic_copy(void* buf,unsigned long bytes);
extern snd_pcm_sframes_t aw_haptic_pointer(struct snd_pcm_substream *substream);
extern int aw_haptic_close(void);
extern int aw_haptic_work_routine(void);

#endif