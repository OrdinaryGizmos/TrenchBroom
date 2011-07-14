//
//  TextRenderer.m
//  TrenchBroom
//
//  Created by Kristian Duske on 13.07.11.
//  Copyright 2011 TU Berlin. All rights reserved.
//

#import "TextRenderer.h"
#import "GLFontManager.h"
#import "GLString.h"
#import "TextAnchor.h"
#import "GLUtils.h"
#import "Camera.h"

@implementation TextRenderer

- (id)initWithFontManager:(GLFontManager *)theFontManager camera:(Camera *)theCamera {
    NSAssert(theFontManager != nil, @"font manager must not be nil");
    NSAssert(theCamera != nil, @"camera must not be nil");
    
    if ((self = [self init])) {
        strings = [[NSMutableDictionary alloc] init];
        anchors = [[NSMutableDictionary alloc] init];
        fontManager = [theFontManager retain];
        camera = [theCamera retain];
    }
    
    return self;
}

- (void)dealloc {
    [fontManager release];
    [strings release];
    [anchors release];
    [super dealloc];
}

- (void)addString:(NSString *)theString forKey:(id <NSCopying>)theKey withFont:(NSFont *)theFont withAnchor:(id <TextAnchor>)theAnchor {
    NSAssert(theString != nil, @"string must not be nil");
    NSAssert(theKey != nil, @"key must not be nil");
    NSAssert(theFont != nil, @"font must not be nil");
    NSAssert(theAnchor != nil, @"anchor must not be nil");
    
    GLString* glString = [fontManager glStringFor:theString font:theFont];
    [strings setObject:glString forKey:theKey];
     [anchors setObject:theAnchor forKey:theKey];
}

- (void)removeStringForKey:(id <NSCopying>)theKey {
    NSAssert(theKey != nil, @"key must not be nil");
    
    [strings removeObjectForKey:theKey];
    [anchors removeObjectForKey:theKey];
}

- (void)renderColor:(float *)theColor {
    TVector3f position;
    
    
    NSEnumerator* keyEn = [strings keyEnumerator];
    id <NSCopying> key;
    while ((key = [keyEn nextObject])) {
        GLString* glString = [strings objectForKey:key];
        id <TextAnchor> anchor = [anchors objectForKey:key];
        
        [anchor position:&position];
        float dist = [camera distanceTo:&position];
        if (dist <= 500) {
            if (dist >= 400) {
                glColor4f(theColor[0], theColor[1], theColor[2], theColor[3] - theColor[3] * (dist - 400) / 100);
            } else {
                glColor4f(theColor[0], theColor[1], theColor[2], theColor[3]);
            }
            float factor = dist / 300;
            
            NSSize size = [glString size];

            glPushMatrix();
            glTranslatef(position.x, position.y, position.z);
            [camera setBillboardMatrix];
            glScalef(factor, factor, 0);
            glTranslatef(-size.width / 2, 0, 0);
            [glString render];
            glPopMatrix();
        }
    }
    
}

@end
