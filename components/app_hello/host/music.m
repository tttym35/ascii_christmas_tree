// SPDX-License-Identifier: Apache-2.0
#import <AVFoundation/AVFoundation.h>

/* Owned by the simulator backend for the lifetime of the preview. */
static AVAudioPlayer *player;
void tree_music_start(void)
{
    @autoreleasepool {
        NSURL *url = [NSURL fileURLWithPath:@TREE_MUSIC_PATH];
        player = [[AVAudioPlayer alloc] initWithContentsOfURL:url error:nil];
        player.numberOfLoops = -1;
        player.volume = 0.3f;
        if (![player play]) NSLog(@"Tree preview audio output unavailable");
    }
}
void tree_music_stop(void)
{
    [player stop];
    player = nil;
}
