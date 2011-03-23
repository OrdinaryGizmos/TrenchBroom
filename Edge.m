//
//  Edge.m
//  TrenchBroom
//
//  Created by Kristian Duske on 24.01.11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#import "Edge.h"
#import "SideEdge.h"
#import "Side.h"
#import "Vertex.h"
#import "Face.h"
#import "MutableFace.h"
#import "Line3D.h"
#import "Plane3D.h"
#import "Ray3D.h"
#import "Vector3f.h"
#import "MathCache.h"
#import "Math.h"
#import "BoundingBox.h"
#import "RenderContext.h"
#import "VBOBuffer.h"
#import "VBOMemBlock.h"
#import "IntData.h"

static float HANDLE_RADIUS = 2.0f;

@implementation Edge
- (id)init {
    if (self = [super init]) {
        mark = EM_NEW;
    }
    
    return self;
}

- (id)initWithStartVertex:(Vertex *)theStartVertex endVertex:(Vertex *)theEndVertex {
    if (theStartVertex == nil)
        [NSException raise:NSInvalidArgumentException format:@"start vertex must not be nil"];
    if (theEndVertex == nil)
        [NSException raise:NSInvalidArgumentException format:@"end vertex must not be nil"];
    
    if (self = [self init]) {
        startVertex = [theStartVertex retain];
        endVertex = [theEndVertex retain];
    }
    
    return self;
}

- (Vertex *)startVertex {
    return startVertex;
}

- (Vertex *)endVertex {
    return endVertex;
}

- (id <Face>)leftFace {
    return [[leftEdge side] face];
}

- (id <Face>)rightFace {
    return [[rightEdge side] face];
}

- (void)setLeftEdge:(SideEdge *)theLeftEdge {
    if (leftEdge != nil)
        [NSException raise:NSInvalidArgumentException format:@"left edge is already set"];
    
    leftEdge = [theLeftEdge retain];
}
- (void)setRightEdge:(SideEdge *)theRightEdge {
    if (rightEdge != nil)
        [NSException raise:NSInvalidArgumentException format:@"right edge is already set"];
    
    rightEdge = [theRightEdge retain];
}

- (Vertex *)splitAt:(Plane3D *)plane {
    if (mark != EM_SPLIT)
        [NSException raise:NSInvalidArgumentException format:@"cannot split edge that is not marked with EM_SPLIT"];

    MathCache* cache = [MathCache sharedCache];
    Line3D* line = [cache line3D];
    [line setPoint1:[startVertex vector] point2:[endVertex vector]];
    
    Vector3f* newVector = [line pointAtDistance:[plane intersectWithLine:line]];
    Vertex* newVertex = [[Vertex alloc] initWithVector:newVector];

    if ([startVertex mark] == VM_DROP) {
        [startVertex release];
        startVertex = [newVertex retain];
    } else {
        [endVertex release];
        endVertex = [newVertex retain];
    }

    [cache returnLine3D:line];
    return [newVertex autorelease];
}

- (PickingHit *)pickWithRay:(Ray3D *)theRay {
    MathCache* cache = [MathCache sharedCache];
    Vector3f* u = [cache vector3f];
    Vector3f* v = [cache vector3f];
    Vector3f* w = [cache vector3f];
    @try {
        [u setFloat:[endVertex vector]];
        [u sub:[startVertex vector]];
        [v setFloat:[theRay direction]];
        [w setFloat:[startVertex vector]];
        [w sub:[theRay origin]];
        float a = [u dot:u];
        float b = [u dot:v];
        float c = [v dot:v];
        float d = [u dot:w];
        float e = [v dot:w];
        float D = a * c - b * b;
        float ec, eN;
        float eD = D;
        float rc, rN;
        float rD = D;
        
        if (fzero(D)) {
            eN = 0;
            eD = 1;
            rN = e;
            rD = c;
        } else {
            eN = b * e - c * d;
            rN = a * e - b * d;
            if (fneg(eN)) { // point is beyond the start point of this edge
                eN = 0;
                rN = e;
                rD = c;
            } else if (fgt(eN, eD)) { // point is beyond the end point of this edge
                eN = eD;
                rN = e + b;
                rD = c;
            }
        }

        if (fneg(rN)) { // point before ray origin
            rN = 0;
            if (fneg(-d)) {
                eN = 0;
            } else if (fgt(-d, a)) {
                eN = eD;
            } else {
                eN = -d;
                eD = a;
            }
        }
        
        ec = fzero(eN) ? 0 : eN / eD;
        rc = fzero(rN) ? 0 : rN / rD;
        
        [u scale:ec];
        [v scale:rc];
        
        [w add:u];
        [w sub:v];
        

        if (fgt([w lengthSquared], HANDLE_RADIUS * HANDLE_RADIUS))
            return nil;

        Vector3f* is = [theRay pointAtDistance:rc];
        return [[[PickingHit alloc] initWithObject:self type:HT_EDGE hitPoint:is distance:rc] autorelease];
    } @finally {
        [cache returnVector3f:u];
        [cache returnVector3f:v];
        [cache returnVector3f:w];
    }
}

- (void)expandBounds:(BoundingBox *)theBounds {
    Vector3f* s = [startVertex vector];
    Vector3f* e = [endVertex vector];
    
    MathCache* cache = [MathCache sharedCache];
    Vector3f* t = [cache vector3f];
    Vector3f* r = [cache vector3f];
    [r setX:HANDLE_RADIUS];
    [r setY:HANDLE_RADIUS];
    [r setZ:HANDLE_RADIUS];
    
    [t setFloat:s];
    [t sub:r];
    [theBounds mergePoint:t];
    
    [t setFloat:s];
    [t add:r];
    [theBounds mergePoint:t];
    
    [t setFloat:e];
    [t sub:r];
    [theBounds mergePoint:t];
    
    [t setFloat:e];
    [t add:r];
    [theBounds mergePoint:t];
    
    [cache returnVector3f:t];
    [cache returnVector3f:r];
}

- (EEdgeMark)mark {
    return mark;
}

- (void)updateMark {
    EVertexMark s = [startVertex mark];
    EVertexMark e = [endVertex mark];
    
    if (s == VM_KEEP && e == VM_KEEP)
        mark = EM_KEEP;
    else if (s == VM_DROP && e == VM_DROP)
        mark = EM_DROP;
    else if ((s == VM_KEEP && e == VM_DROP) ||
             (s == VM_DROP && e == VM_KEEP))
        mark = EM_SPLIT;
    else
        mark = EM_UNKNOWN;
}


- (NSString *)description {
    NSMutableString* desc = [NSMutableString stringWithFormat:@"[start: %@, end: %@", startVertex, endVertex];
    switch (mark) {
        case EM_KEEP:
            [desc appendFormat:@", mark: EM_KEEP]"];
            break;
        case EM_DROP:
            [desc appendFormat:@", mark: EM_DROP]"];
            break;
        case EM_SPLIT:
            [desc appendFormat:@", mark: EM_SPLIT]"];
            break;
        case EM_NEW:
            [desc appendFormat:@", mark: EM_NEW]"];
            break;
        case EM_UNKNOWN:
            [desc appendFormat:@", mark: EM_UNKNOWN]"];
            break;
        default:
            [desc appendFormat:@", mark: invalid]"];
            break;
    }
    
    return desc;
}

- (void)dealloc {
    [startVertex release];
    [endVertex release];
    [leftEdge release];
    [rightEdge release];
    [super dealloc];
}

@end
