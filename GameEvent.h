#pragma once
#include <functional>
#include <vector>
#include <utility>
#include <SimpleMath.h>
#include <winnt.h>

//ゲームでのイベントを知らせる仕組み
namespace GameEvent
{
	//イベントの中身
	struct Judge
	{
		int lane;	//0~5
		int rank;	//0=weak 1=hit 2=graze
		DirectX::SimpleMath::Vector3 pos;	//判定円の位置
		DirectX::SimpleMath::Vector3 color;	//レーンの色
	};
	struct Beat	//拍の頭
	{
		long long index;	//曲の頭から何拍めか
		bool isBar;			//小節の頭か
	};
	struct LongEnd { int lane; };	//ロングを最後まで押し切った
	struct ShieldBreak { DirectX::SimpleMath::Vector3 center; };	//ロングの盾が割れた

	//知らせる
	template<class T>
	class Channel
	{
	public:
		using Handler = std::function<void(const T&)>;

		//聞く。戻り値の番号は聞くのをやめるときに使用
		int Subscribe(Handler h)
		{
			int id = ++m_NextId;
			m_Handlers.push_back({ id,std::move(h) });
			return id;
		}
		//聞くの終了
		void Unsubscribe(int id)
		{
			for(auto it = m_Handlers.begin(); it != m_Handlers.end(); ++it)
				if (it->first == id) { m_Handlers.erase(it); return; }
		}
		//知らせる　聞いている全員の関数を呼ぶ
		void Publish(const T& e) const
		{
			auto copy = m_Handlers;	//呼んだ先で聞くものが増えても壊れないように写しを回すための機構
			for (auto& h : copy) h.second(e);
		}
		void Clear() { m_Handlers.clear(); }

	private:
		std::vector<std::pair<int, Handler>>m_Handlers;
		int m_NextId = 0;
	};

	inline Channel<Judge>& OnJudge() { static Channel<Judge> c; return c; }
	inline Channel<Beat>& OnBeat() { static Channel<Beat> c; return c; }
	inline Channel<LongEnd>& OnLongEnd() { static Channel<LongEnd> c; return c; }
	inline Channel<ShieldBreak>& OnShieldBreak() { static Channel<ShieldBreak> c; return c; }

	inline void ClearAll()
	{
		OnJudge().Clear();
		OnBeat().Clear();
		OnLongEnd().Clear();
		OnShieldBreak().Clear();
	}
}


