#include "main.hpp"
#include <cmath>
#include <math.h>
#include "al/execute/ExecuteOrder.h"
#include "al/execute/ExecuteTable.h"
#include "al/execute/ExecuteTableHolderDraw.h"
#include "al/util/GraphicsUtil.h"
#include "container/seadSafeArray.h"
#include "game/GameData/GameDataHolderAccessor.h"
#include "game/Player/PlayerActorBase.h"
#include "game/Player/PlayerActorHakoniwa.h"
#include "game/Player/PlayerHackKeeper.h"
#include "heap/seadHeap.h"
#include "math/seadVector.h"
#include "server/Client.hpp"
#include "puppets/PuppetInfo.h"
#include "actors/PuppetActor.h"
#include "al/LiveActor/LiveActor.h"
#include "al/util.hpp"
#include "al/util/AudioUtil.h"
#include "al/util/CameraUtil.h"
#include "al/util/ControllerUtil.h"
#include "al/util/LiveActorUtil.h"
#include "al/util/NerveUtil.h"
#include "debugMenu.hpp"
#include "game/GameData/GameDataFunction.h"
#include "game/HakoniwaSequence/HakoniwaSequence.h"
#include "game/Player/PlayerFunction.h"
#include "game/StageScene/StageScene.h"
#include "helpers.hpp"
#include "layouts/HideAndSeekIcon.h"
#include "logger.hpp"
#include "rs/util.hpp"
#include "server/gamemode/GameModeBase.hpp"
#include "server/hns/HideAndSeekMode.hpp"
#include "server/gamemode/GameModeManager.hpp"

static int pInfSendTimer = 0;
static int gameInfSendTimer = 0;

void updatePlayerInfo(GameDataHolderAccessor holder, PlayerActorBase* playerBase, bool isYukimaru) {
    
    if (pInfSendTimer >= 3) {

        Client::sendPlayerInfPacket(playerBase, isYukimaru);

        if (!isYukimaru) {
            Client::sendHackCapInfPacket(((PlayerActorHakoniwa*)playerBase)->mHackCap);

            Client::sendCaptureInfPacket((PlayerActorHakoniwa*)playerBase);
        }

        pInfSendTimer = 0;
    }

    if (gameInfSendTimer >= 60) {

        if (isYukimaru) {
            Client::sendGameInfPacket(holder);
        } else {
            Client::sendGameInfPacket((PlayerActorHakoniwa*)playerBase, holder);
        }
        
        gameInfSendTimer = 0;
    }

    pInfSendTimer++;
    gameInfSendTimer++;
}

// ------------- Hooks -------------

int debugPuppetIndex = 0;
int debugCaptureIndex = 0;
static int pageIndex = 0;

static const int maxPages = 3;

void drawMainHook(HakoniwaSequence *curSequence, sead::Viewport *viewport, sead::DrawContext *drawContext) {

    // sead::FrameBuffer *frameBuffer;
    // __asm ("MOV %[result], X21" : [result] "=r" (frameBuffer));

    // if(Application::sInstance->mFramework) {
    //     Application::sInstance->mFramework->mGpuPerf->drawResult((agl::DrawContext *)drawContext, frameBuffer);
    // }

    Time::calcTime();  // this needs to be ran every frame, so running it here works

    if (!debugMode) {
        al::executeDraw(curSequence->mLytKit, "２Ｄバック（メイン画面）");
        return;
    }

    Client* client = Client::instance();
    SocketClient* socket = client->mSocket;
    bool isConnected = socket->isConnected();

    // int dispWidth = al::getLayoutDisplayWidth();
    int dispHeight = al::getLayoutDisplayHeight();

    gTextWriter->mViewport = viewport;

    gTextWriter->mColor = sead::Color4f(1.f, 1.f, 1.f, 0.8f);

    drawBackground((agl::DrawContext *)drawContext);

    gTextWriter->beginDraw();
    gTextWriter->setCursorFromTopLeft(sead::Vector2f(10.f, 10.f));

    gTextWriter->printf("FPS: %d\n", static_cast<int>(round(Application::sInstance->mFramework->calcFps())));

    gTextWriter->setCursorFromTopLeft(sead::Vector2f(10.f, (dispHeight / 3) + 30.f));
    gTextWriter->setScaleFromFontHeight(20.f);

    gTextWriter->printf("Your TCP status: %s\n", socket->getStateChar());
    gTextWriter->printf("Your UDP status: %s\n", socket->getUdpStateChar());
    gTextWriter->printf("Connected Players: %d/%d\n", Client::getConnectCount() + (isConnected ? 1 : 0), Client::getMaxPlayerCount());

    sead::Heap* clientHeap = Client::getClientHeap();
    if (clientHeap) {
        sead::Heap *gmHeap = GameModeManager::instance()->getHeap();
        gTextWriter->printf(
            "Heap Use: %.1f/%.0f (Client) %.1f/%.0f (Gmode)\n",
            0.0009765625 * (clientHeap->getSize() - clientHeap->getFreeSize()),
            0.0009765625 * clientHeap->getSize(),
            0.0009765625 * (gmHeap->getSize() - gmHeap->getFreeSize()),
            0.0009765625 * gmHeap->getSize()
        );
    }

    gTextWriter->printf(
        "Queue Count: %d/%d (Send) %d/%d (Receive)\n",
        socket->getSendCount(),
        socket->getSendMaxCount(),
        socket->getRecvCount(),
        socket->getRecvMaxCount()
    );

#if EMU
    gTextWriter->printf("Mod version: %s for Emulators\n", TOSTRING(BUILDVERSTR));
#else
    gTextWriter->printf("Mod version: %s for Switch\n", TOSTRING(BUILDVERSTR));
#endif

    al::Scene *curScene = curSequence->curScene;

    if (curScene && isInGame) {
        sead::LookAtCamera *cam = al::getLookAtCamera(curScene, 0);
        sead::Projection* projection = al::getProjectionSead(curScene, 0);

        PlayerActorBase* playerBase = rs::getPlayerActor(curScene);

        PuppetActor* curPuppet = Client::getPuppet(debugPuppetIndex - 1);
        PuppetActor *debugPuppet = Client::getDebugPuppet();
        if (debugPuppet) {
            curPuppet = debugPuppet;
        }

        sead::PrimitiveRenderer *renderer = sead::PrimitiveRenderer::instance();
        renderer->setDrawContext(drawContext);
        renderer->setCamera(*cam);
        renderer->setProjection(*projection);

        gTextWriter->printf("(ZR ←)------------ Page %d/%d -------------(ZR →)\n", pageIndex + 1, maxPages);

        switch (pageIndex)
        {
        case 0:
            {
                gTextWriter->printf(
                    "(ZL ←)----------%s Player %d/%d %s-----------(ZL →)\n\n",
                    debugPuppetIndex + 1 < 10 ? "-" : "",
                    debugPuppetIndex + 1,
                    Client::getMaxPlayerCount(),
                    Client::getMaxPlayerCount() < 10 ? "-" : ""
                );

                if (debugPuppetIndex == 0) {
                    gTextWriter->printf("Player Name: %s\n",       Client::getClientName());
                    gTextWriter->printf("Connection Status: %s\n", isConnected ? "Online" : "Offline");
                    gTextWriter->printf("Is in same Stage: Yes\n");
                    gTextWriter->printf("Stage: %s\n",            client->getLastGameInfPacket()->stageName);
                    gTextWriter->printf("Scenario: %u\n",         client->getLastGameInfPacket()->scenarioNo);
                    gTextWriter->printf("Costume: H: %s B: %s\n", client->getLastCostumeInfPacket()->capModel, client->getLastCostumeInfPacket()->bodyModel);
                    gTextWriter->printf("Capture: %s\n",          client->getLastCaptureInfPacket()->hackName);

                    PlayerHackKeeper* hackKeeper = playerBase->getPlayerHackKeeper();
                    if (hackKeeper) {
                        PlayerActorHakoniwa *p1 = (PlayerActorHakoniwa*)playerBase;
                        if (hackKeeper->currentHackActor) {
                            gTextWriter->printf("Animation: %s\n", al::getActionName(hackKeeper->currentHackActor));
                        } else {
                            gTextWriter->printf("Animation: %s\n", p1->mPlayerAnimator->mAnimFrameCtrl->getActionName());
                        }
                    }
                } else if (curPuppet) {
                    al::LiveActor* curModel = curPuppet->getCurrentModel();

                    PuppetInfo* curPupInfo = curPuppet->getInfo();

                    if (curModel && curPupInfo) {
                        gTextWriter->printf("Player Name: %s\n",       curPupInfo->puppetName);
                        gTextWriter->printf("Connection Status: %s\n", curPupInfo->isConnected ? "Online" : "Offline");
                        gTextWriter->printf("Is in same Stage: %s\n",  curPupInfo->isInSameStage ? "Yes" : "No");
                        gTextWriter->printf("Stage: %s\n",             curPupInfo->stageName);
                        gTextWriter->printf("Scenario: %u\n",          curPupInfo->scenarioNo);
                        gTextWriter->printf("Costume: H: %s B: %s\n",  curPupInfo->costumeHead, curPupInfo->costumeBody);
                        gTextWriter->printf("Capture: %s\n",           curPupInfo->isCaptured ? curPupInfo->curHack : "");
                        gTextWriter->printf("Animation:  %d  %s\n",    curPupInfo->curAnim, curPupInfo->curAnimStr);
                        if (!curPupInfo->isCaptured) {
                            gTextWriter->printf("Model Animation: %s\n", al::getActionName(curModel));
                        }
                    }
                }
            }
            break;
        case 1:
            {
                gTextWriter->printf("----------------- Debug Puppet ------------------\n\n");
                PuppetActor* debugPuppet = Client::getDebugPuppet();
                PuppetInfo* debugInfo = Client::getDebugPuppetInfo();

                if (debugPuppet && debugInfo) {

                    al::LiveActor *curModel = debugPuppet->getCurrentModel();

                    gTextWriter->printf("Is Debug Puppet Tagged: %s\n", BTOC(debugInfo->isIt));

                }
            }
            break;
        case 2:
            {
                gTextWriter->printf("--------------- Animation & Cappy ---------------\n\n");
                PlayerHackKeeper* hackKeeper = playerBase->getPlayerHackKeeper();

                if (hackKeeper) {
                    PlayerActorHakoniwa *p1 = (PlayerActorHakoniwa*)playerBase; // its safe to assume that we're using a playeractorhakoniwa if the hack keeper isnt null

                    if (hackKeeper->currentHackActor) {
                        al::LiveActor *curHack = hackKeeper->currentHackActor;

                        gTextWriter->printf("Current Hack Animation: %s\n", al::getActionName(curHack));
                        gTextWriter->printf("Current Hack Name: %s\n", hackKeeper->getCurrentHackName());
                        sead::Quatf captureRot = curHack->mPoseKeeper->getQuat();
                        gTextWriter->printf("Current Hack Rot: %.3f %.3f %.3f %f\n", captureRot.x, captureRot.y, captureRot.z, captureRot.w);
                        sead::Quatf calcRot;
                        al::calcQuat(&calcRot, curHack);
                        gTextWriter->printf("Calc Hack Rot: %.3f %.3f %.3f %.3f\n", calcRot.x, calcRot.y, calcRot.z, calcRot.w);
                    } else {
                        gTextWriter->printf("Cur Action: %s\n", p1->mPlayerAnimator->mAnimFrameCtrl->getActionName());
                        gTextWriter->printf("Cur Sub Action: %s\n", p1->mPlayerAnimator->curSubAnim.cstr());
                        gTextWriter->printf("Is Cappy Flying? %s\n", BTOC(p1->mHackCap->isFlying()));
                        if (p1->mHackCap->isFlying()) {
                            gTextWriter->printf("Cappy Action: %s\n", al::getActionName(p1->mHackCap));
                            sead::Vector3f *capTrans = al::getTransPtr(p1->mHackCap);
                            sead::Vector3f *capRot = &p1->mHackCap->mJointKeeper->mJointRot;
                            gTextWriter->printf("Cappy: Position   Rotation\nX:   % 10.3f % 10.3f\nY:   % 10.3f % 10.3f\nZ:   % 10.3f % 10.3f\n",
                                capTrans->x, capRot->x,
                                capTrans->y, capRot->y,
                                capTrans->z, capRot->z
                            );
                            gTextWriter->printf("Cappy Skew: %.3f\n", p1->mHackCap->mJointKeeper->mSkew);
                        }
                    }
                }
            }
            break;
        default:
            break;
        }

        renderer->begin();

        //sead::Matrix34f mat = sead::Matrix34f::ident;
        //mat.setBase(3, sead::Vector3f::zero); // Sets the position of the matrix.
                             // For cubes, you need to put this at the location.
                             // For spheres, you can leave this at 0 0 0 since you set it in its draw function.
        renderer->setModelMatrix(sead::Matrix34f::ident);

        if (curPuppet) {
            renderer->drawSphere4x8(curPuppet->getInfo()->playerPos, 20, sead::Color4f(1.f, 0.f, 0.f, 0.25f));
            renderer->drawSphere4x8(al::getTrans(curPuppet), 20, sead::Color4f(0.f, 0.f, 1.f, 0.25f));
        } else if (debugPuppetIndex == 0) {
            renderer->drawSphere4x8(client->getLastPlayerInfPacket()->playerPos, 20, sead::Color4f(1.f, 0.f, 0.f, 0.25f));
        }

        renderer->end();

        isInGame = false;
    }

    gTextWriter->endDraw();

    al::executeDraw(curSequence->mLytKit, "２Ｄバック（メイン画面）");

}

void sendShinePacket(GameDataHolderAccessor thisPtr, Shine* curShine) {

    if (!curShine->isGot()) {

        GameDataFile::HintInfo* curHintInfo =
            &thisPtr.mData->mGameDataFile->mShineHintList[curShine->mShineIdx];

        Client::sendShineCollectPacket(curHintInfo->mUniqueID);
    }

    GameDataFunction::setGotShine(thisPtr, curShine->curShineInfo);
}

void stageInitHook(al::ActorInitInfo *info, StageScene *curScene, al::PlacementInfo const *placement, al::LayoutInitInfo const *lytInfo, al::ActorFactory const *factory, al::SceneMsgCtrl *sceneMsgCtrl, al::GameDataHolderBase *dataHolder) {

    al::initActorInitInfo(info, curScene, placement, lytInfo, factory, sceneMsgCtrl,
                          dataHolder);

    Client::clearArrays();

    Client::setSceneInfo(*info, curScene);

    if (GameModeManager::instance()->getGameMode() != NONE) {
        GameModeInitInfo initModeInfo(info, curScene);
        initModeInfo.initServerInfo(GameModeManager::instance()->getGameMode(), Client::getPuppetHolder());

        GameModeManager::instance()->initScene(initModeInfo);
    }

    Client::sendGameInfPacket(info->mActorSceneInfo.mSceneObjHolder);

}

PlayerCostumeInfo *setPlayerModel(al::LiveActor *player, const al::ActorInitInfo &initInfo, const char *bodyModel, const char *capModel, al::AudioKeeper *keeper, bool isCloset) {
    Client::sendCostumeInfPacket(bodyModel, capModel);
    return PlayerFunction::initMarioModelActor(player, initInfo, bodyModel, capModel, keeper, isCloset);
}

al::SequenceInitInfo* initInfo;

ulong constructHook() {  // hook for constructing anything we need to globally be accesible

    __asm("STR X21, [X19,#0x208]"); // stores WorldResourceLoader into HakoniwaSequence

    __asm("MOV %[result], X20"
          : [result] "=r"(
              initInfo));  // Save our scenes init info to a gloabl ptr so we can access it later

    Client::createInstance(al::getCurrentHeap());
    GameModeManager::createInstance(al::getCurrentHeap()); // Create the GameModeManager on the current al heap

    return 0x20;
}

bool threadInit(HakoniwaSequence *mainSeq) {  // hook for initializing client class

    al::LayoutInitInfo lytInfo = al::LayoutInitInfo();

    al::initLayoutInitInfo(&lytInfo, mainSeq->mLytKit, 0, mainSeq->mAudioDirector, initInfo->mSystemInfo->mLayoutSys, initInfo->mSystemInfo->mMessageSys, initInfo->mSystemInfo->mGamePadSys);

    Client::instance()->init(lytInfo, mainSeq->mGameDataHolder);

    return GameDataFunction::isPlayDemoOpening(mainSeq->mGameDataHolder);
}

bool hakoniwaSequenceHook(HakoniwaSequence* sequence) {
    StageScene* stageScene = (StageScene*)sequence->curScene;

    static bool isCameraActive = false;

    bool isFirstStep = al::isFirstStep(sequence);

    al::PlayerHolder *pHolder = al::getScenePlayerHolder(stageScene);
    PlayerActorBase* playerBase = al::tryGetPlayerActor(pHolder, 0);
    
    bool isYukimaru = !playerBase->getPlayerInfo();

    isInGame = !stageScene->isPause();

    GameModeManager::instance()->setPaused(stageScene->isPause());
    Client::setStageInfo(stageScene->mHolder);

    Client::update();

    updatePlayerInfo(stageScene->mHolder, playerBase, isYukimaru);

    static bool isDisableMusic = false;

    if (al::isPadHoldZR(-1)) {
        if (al::isPadTriggerUp(-1)) debugMode = !debugMode;
        if (al::isPadTriggerLeft(-1)) pageIndex--;
        if (al::isPadTriggerRight(-1)) pageIndex++;
        if(pageIndex < 0) {
            pageIndex = maxPages - 1;
        }
        if(pageIndex >= maxPages) pageIndex = 0;

    } else if (al::isPadHoldZL(-1)) {

        if (debugMode) {
            if (al::isPadTriggerLeft(-1)) debugPuppetIndex--;
            if (al::isPadTriggerRight(-1)) debugPuppetIndex++;

            if (debugPuppetIndex < 0) {
                debugPuppetIndex = Client::getMaxPlayerCount() - 1;
            }
            if (debugPuppetIndex >= Client::getMaxPlayerCount()) {
                debugPuppetIndex = 0;
            }
        }

    } else if (al::isPadHoldL(-1)) {
        if (al::isPadTriggerLeft(-1)) GameModeManager::instance()->toggleActive();
        if (al::isPadTriggerRight(-1)) {
            if (debugMode) {
                
                PuppetInfo* debugPuppet = Client::getDebugPuppetInfo();
                
                if (debugPuppet) {

                    debugPuppet->playerPos = al::getTrans(playerBase);
                    al::calcQuat(&debugPuppet->playerRot, playerBase);

                    PlayerHackKeeper* hackKeeper = playerBase->getPlayerHackKeeper();

                    if (hackKeeper) {
                        const char *hackName = hackKeeper->getCurrentHackName();
                        debugPuppet->isCaptured = hackName != nullptr;
                        if (debugPuppet->isCaptured) {
                            strcpy(debugPuppet->curHack, hackName);
                        } else {
                            strcpy(debugPuppet->curHack, "");
                        }
                    }
                    
                }
            }
        }
        if (al::isPadTriggerUp(-1)) {
            if (debugMode) {
                PuppetActor* debugPuppet = Client::getDebugPuppet();
                if (debugPuppet) {
                    PuppetInfo *info = debugPuppet->getInfo();
                    // info->isIt = !info->isIt;

                    debugPuppet->emitJoinEffect();
                    
                }
            } else {
                isDisableMusic = !isDisableMusic;
            }
        }
    }

    if (isDisableMusic) {
        if (al::isPlayingBgm(stageScene)) {
            al::stopAllBgm(stageScene, 0);
        }
    }

    return isFirstStep;

}

void seadPrintHook(const char *fmt, ...)
{
    va_list args;
	va_start(args, fmt);

    Logger::log(fmt, args);

    va_end(args);
}