#include "GameCharacter.h"
#include "GameCharacterState.h"
#include "FlightProps.h"
#include "MessageDispatcher.h"

GameCharacter* GameCharacter::create(int id)
{
    /**
    	 临时就创建一种角色，这里是将组成一个游戏角色的部分拼接在一起
    */
    auto tmpRet = new GameCharacter();
    tmpRet->autorelease();

    /**
    	 Fuck主要是以后会有多种人物，这里吧，暂时就这样搞
    */
    /**
        在此处拼装状态机、外形等
    */
    tmpRet->m_stateMachine  =   StateMachine<GameCharacter>::create(tmpRet);
    tmpRet->m_stateMachine->retain();
    switch (id)
    {
    case 1:                                                 // 对应的是宙斯
        {
            // 不同的角色有不同的外形
            tmpRet->m_shape         =   GameCharacterShape::create("zhousi.ExportJson", "zhousi");
            tmpRet->m_shape->retain();

            // 不同的角色有不同的状态转换表
            tmpRet->m_stateMachine->changeState(GameCharacterIdleState::create());
            tmpRet->m_stateMachine->setGlobalState(GameCharacterGlobalState::create());

            // 不同的角色有不同的初始属性
            tmpRet->m_attribute     =   GameCharacterAttribute(200, 10, 30, 70);

            // 角色类型，宙斯属于近程攻击单位
            tmpRet->m_characterType =   GAMECHARACTER_TYPE_ENUM_SHORT_RANGE;

            break;
        }

    case 2:                                                 // 精灵
        {
            tmpRet->m_shape         =   GameCharacterShape::create("xuejingling-qian.ExportJson", "xuejingling-qian");
            tmpRet->m_shape->retain();
            // 另一套状态转换表@_@ 以后看能不能将这部分的AI逻辑移到lua中去，不然坑死我了
            // 目前先使用一套AI逻辑@_@  FUCK
            // auto tmpState = GameCharacterPursueState::create();
            // tmpState->targetId  =   4;
            tmpRet->m_stateMachine->changeState(GameCharacterIdleState::create());
            tmpRet->m_stateMachine->setGlobalState(GameCharacterGlobalState::create());

            tmpRet->m_attribute     =   GameCharacterAttribute(100, 40, 10, 90, 700);
            
            // 雪精灵远程攻击单位
            tmpRet->m_characterType =   GAMECHARACTER_TYPE_ENUM_LONG_RANGE;

            break;
        }

    case 3:                                                 // 骑士
        {
            tmpRet->m_shape         =   GameCharacterShape::create("Aer.ExportJson", "Aer");
            tmpRet->m_shape->retain();

            tmpRet->m_stateMachine->changeState(GameCharacterIdleState::create());
            tmpRet->m_stateMachine->setGlobalState(GameCharacterGlobalState::create());

            tmpRet->m_attribute     =   GameCharacterAttribute(150, 20, 20, 110);
            
            // 骑士：进程攻击单位
            tmpRet->m_characterType =   GAMECHARACTER_TYPE_ENUM_SHORT_RANGE;

            break;
        }

    case 4:                                                 // 野猪怪
        {
            tmpRet->m_shape         =   GameCharacterShape::create("Pig.ExportJson", "Pig");
            tmpRet->m_shape->retain();

            tmpRet->m_stateMachine->changeState(GameCharacterIdleState::create());
            tmpRet->m_stateMachine->setGlobalState(GameCharacterGlobalState::create());

            tmpRet->m_attribute     =   GameCharacterAttribute(10, 1, 10, 60 + CCRANDOM_0_1() * 20);

            // 野猪怪：近程攻击单位
            tmpRet->m_characterType =   GAMECHARACTER_TYPE_ENUM_SHORT_RANGE;

            break;
        }

    default:
        break;
    }
    
    
    return tmpRet;
}

GameCharacter::GameCharacter()
{
    m_stateMachine                  =   nullptr;
    m_shape                         =   nullptr;
    m_graph                         =   nullptr;
    m_moveAction                    =   nullptr;
    m_frameCount                    =   0;
    m_lastExitNormalAttackFrame     =   0;
}

GameCharacter::~GameCharacter()
{
    // 移除该角色在网格中所占的坑位
    m_graph->removeObjectFromGrid(&m_objectOnGrid);
    CC_SAFE_RELEASE_NULL(m_stateMachine);

    // 将该角色的显示从显示列表中移除
    m_shape->removeFromParent();
    CC_SAFE_RELEASE_NULL(m_shape);
}

void GameCharacter::update(float dm)
{
    /**
    * 这里严重注意：在状态机中可能会删除自己，比如调用die的时候 
    */
    m_frameCount++;
    m_stateMachine->update(dm);
}

bool GameCharacter::handleMessage(Telegram& msg)
{
    return m_stateMachine->handleMessage(msg);
}

StateMachine<GameCharacter>* GameCharacter::getFSM()
{
    return m_stateMachine;
}

GameCharacterShape* GameCharacter::getShape()
{
    return m_shape;
}

void GameCharacter::setGridGraph(MapGrid* graph)
{
    m_graph =   graph;
}

ObjectOnMapGrid* GameCharacter::getObjectOnGrid()
{
    return &m_objectOnGrid;
}

void GameCharacter::moveToGridIndex(int nodeIndex, float rate)
{
    // 如果移动目标是无效的格子索引
    if (nodeIndex == INVALID_NODE_INDEX)
    {
        return;
    }

    auto tmpResourceGrid    =   m_graph->getNodeByIndex(m_objectOnGrid.nodeIndex);
    auto tmpTargetGird      =   m_graph->getNodeByIndex(nodeIndex);

    // 先修改之前占领的网格
    m_graph->removeObjectFromGrid(&m_objectOnGrid);
    m_objectOnGrid.nodeIndex    =   nodeIndex;
    m_graph->addObjectToGrid(&m_objectOnGrid);

    // 首先结束之前的动作
    if (m_moveAction != nullptr)
    {
        m_shape->stopAction(m_moveAction);
        onMoveOver(nullptr);
    }

    // 设置缓动过去
    auto tmpDirection       =   (tmpTargetGird.getX() - tmpResourceGrid.getX()) * (tmpTargetGird.getX() - tmpResourceGrid.getX()) + 
         (tmpTargetGird.getY() - tmpResourceGrid.getY()) * (tmpTargetGird.getY() - tmpResourceGrid.getY());
    tmpDirection        =   sqrt(tmpDirection);
    m_moveAction        =   MoveTo::create(tmpDirection / rate, Vec2(tmpTargetGird.getX(), tmpTargetGird.getY()));
    // 增加一个动作结束的回调
    m_shape->runAction(Sequence::create(m_moveAction, CallFuncN::create(std::bind(&GameCharacter::onMoveOver, this, std::placeholders::_1)), nullptr));
}

MapGrid* GameCharacter::getMapGrid()
{
    return m_graph;
}

bool GameCharacter::isMoving()
{
    return m_moveAction != nullptr;
}

void GameCharacter::onMoveOver(Node* pNode)
{
    m_moveAction    =   nullptr;
}

GameCharacterAttribute& GameCharacter::getAttribute()
{
    return m_attribute;
}

void GameCharacter::die()
{
    // 目前就直接删除掉自己吧
    CC_SAFE_RELEASE(this);
}

GameCharacterTypeEnum GameCharacter::getCharacterType()
{
    return m_characterType;
}

void GameCharacter::normalAttack(int id)
{
    m_normatAttTargetId =   id;
    if (this->getCharacterType() == GAMECHARACTER_TYPE_ENUM_SHORT_RANGE)
    {
        // 对于近程攻击单位，只用播放动画就OK了
        this->getShape()->playAction(ATTACK_ACTION, false, std::bind(&GameCharacter::onShortAttEffect, this, std::placeholders::_1));
    }
    else if (this->getCharacterType() == GAMECHARACTER_TYPE_ENUM_LONG_RANGE)
    {
        // 远程攻击单位，还需要在特定动画的位置丢出大便
        this->getShape()->playAction(ATTACK_ACTION, false, std::bind(&GameCharacter::onLongAttLaunch, this, std::placeholders::_1));
    }
}

void GameCharacter::onShortAttEffect(string evt)
{
    // 给对方发出消息
    auto tmpMsg = TelegramNormalAttack::create(this->getId(), m_normatAttTargetId, this->getAttribute());
    Dispatch->dispatchMessage(*tmpMsg);
}

void GameCharacter::onLongAttLaunch(string evt)
{
    // 丢出大便
    // 目标还在
    if (EntityMgr->getEntityFromID(m_normatAttTargetId) != nullptr)
    {
        auto tmpSnowBall    =   OneToOneArmatureFlightProps::create(this->getId(), m_normatAttTargetId, 1);
        tmpSnowBall->setPosition(this->getShape()->getCenterPos());
        this->getShape()->getParent()->addChild(tmpSnowBall);
    }
}

bool GameCharacter::isNormalAttackFinish()
{
    // 目前就用不在动画中来作为结束
    return this->getShape()->isNotInAnimation();
}

bool GameCharacter::isInAttackDistance(GameCharacter* other)
{
    // 近程攻击单位判断是否在攻击范围的方法和远程攻击单位判断方法不同
    if (this->getCharacterType() == GAMECHARACTER_TYPE_ENUM_SHORT_RANGE)
    {
        // 近程攻击单位
        auto tmpOwnerObject     =   this->getObjectOnGrid();
        auto tmpTargetObject    =   other->getObjectOnGrid();

        // 如果在一条水平线上，并且间距在3个格子内
        if (this->getMapGrid()->testTwoIndexInOneHorizon(tmpOwnerObject->nodeIndex, tmpTargetObject->nodeIndex) 
            && abs(tmpTargetObject->nodeIndex - tmpOwnerObject->nodeIndex) <= 3)
        {
            return true;
        }

        // 如果在交叉位置处
        if (tmpOwnerObject->nodeIndex == other->getMapGrid()->getLeftTopGridIndex(tmpTargetObject->nodeIndex)
            || tmpOwnerObject->nodeIndex == other->getMapGrid()->getRightTopGridIndex(tmpTargetObject->nodeIndex)
            || tmpOwnerObject->nodeIndex == other->getMapGrid()->getRightBottomGridIndex(tmpTargetObject->nodeIndex)
            || tmpOwnerObject->nodeIndex == other->getMapGrid()->getLeftBottomGridIndex(tmpTargetObject->nodeIndex))
        {
            return true;
        }
    }
    else if (this->getCharacterType() == GAMECHARACTER_TYPE_ENUM_LONG_RANGE)
    {
        // 远程攻击单位以半径判断
        return (this->getShape()->getPosition() - other->getShape()->getPosition()).getLength() 
            <= other->getAttribute().getAttDistance();
    }

    return false;
}
void GameCharacter::walkOff()
{
    // 切换动画
    this->getShape()->playAction(RUN_ACTION);
    this->getShape()->faceToRight();

    // 缓动
    Vec2 tmpStartPos    =   this->getShape()->getPosition();
    Vec2 tmpTargetPos   =   tmpStartPos;
    tmpTargetPos.x      =   m_graph->getContentSize().width + 200;
    auto tmpDirection   =   tmpTargetPos.x - tmpStartPos.x;
    if (m_moveAction != nullptr)
    {
        m_shape->stopAction(m_moveAction);
    }
    m_moveAction        =   MoveTo::create(tmpDirection / this->m_attribute.getRate(), tmpTargetPos);
    m_shape->runAction(Sequence::create(m_moveAction, 
        CallFuncN::create(std::bind(&GameCharacter::onMoveOver, this, std::placeholders::_1)), nullptr));
}

void GameCharacter::exitNormalAttack()
{
    m_lastExitNormalAttackFrame =   m_frameCount;
}

bool GameCharacter::canNormalAttack()
{
    return (m_frameCount - m_lastExitNormalAttackFrame) > m_attribute.getAttInterval();
}

int GameCharacter::getNextNormatAttackLeftCount()
{
    return m_attribute.getAttInterval() - (m_frameCount - m_lastExitNormalAttackFrame) + 1;
}

vector<GameCharacter*> GameCharacter::getCharactersInView()
{
    vector<GameCharacter*>  pRet;
    
    /**
    *  @_@ 觉得当前在场上的角色的数量肯定是小于探索范围内网格的数量，所以还是遍历所有的
    *  角色吧，比如范围是15，那么可能的网格数量就有5 * 30 = 150，但是一个地图肯定没有这么多人
    */
    auto tmpIterator    =   EntityMgr->getEntityMap()->begin();
    for (; tmpIterator != EntityMgr->getEntityMap()->end(); )
    {
        auto tmpCharacter   =   dynamic_cast<GameCharacter*>(tmpIterator->second);
        if (m_graph->isInScope(m_objectOnGrid.nodeIndex, tmpCharacter->getObjectOnGrid()->nodeIndex, m_attribute.getViewDistance()))
        {
            pRet.push_back(tmpCharacter);
        }
    }

    return pRet;
}
