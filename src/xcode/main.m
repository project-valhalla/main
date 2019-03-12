#import <Cocoa/Cocoa.h>
#import "SDL2/SDL.h"
#import "Launcher.h"

@interface SDLApplication : NSApplication
@end

@implementation SDLApplication
@end

#ifdef main
#  undef main
#endif

int main(int argc, char *argv[])
{
    BOOL server = NO;
    int i;
    for(i = 0; i < argc; i++) if(strcmp(argv[i], "-d") == 0) { server = YES; break; }
    if(server) return SDL_main(argc, (char**)argv);
    
	// neither of the followin is necessary as the principal class is set to SDLApplication in the plist 
	//[SDLApplication sharedApplication];
    //[SDLApplication poseAsClass:[NSApplication class]];
	
    return NSApplicationMain(argc,  (const char **)argv);
}
