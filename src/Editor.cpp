/*=======================================================================
*
*   Copyright (C) 2013 Lysine.
*
*   Filename:    Editor.cpp
*   Time:        2013/06/30
*   Author:      Lysine
*
*   Lysine is a student majoring in Software Engineering
*   from the School of Software, SUN YAT-SEN UNIVERSITY.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.

*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#include "Editor.h"

Editor::Widget::Widget(QWidget *parent):
	QWidget(parent)
{
	scale=0;
	length=100;
	current=VPlayer::instance()->getTime();
	load();
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this,&Widget::customContextMenuRequested,[this](QPoint p){
		int i=p.y()/length;
		QMenu menu(this);
		connect(menu.addAction(Editor::tr("Delete")),&QAction::triggered,[this,i](){
			auto &p=Danmaku::instance()->getPool();
			p.removeAt(i);
			Danmaku::instance()->parse(0x1|0x2|0x4);
		});
		menu.exec(mapToGlobal(p));
	});
	connect(Danmaku::instance(),&Danmaku::layoutChanged,this,&Widget::load);
}

void Editor::Widget::load()
{
	duration=VPlayer::instance()->getDuration();
	const QList<Record> &pool=Danmaku::instance()->getPool();
	for(QLineEdit *iter:time){
		delete iter;
	}
	time.clear();
	for(int i=0;i<pool.size();++i){
		const Record &line=pool[i];
		bool f=true;
		qint64 t;
		for(const Comment &c:line.danmaku){
			t=f?c.time:qMax(c.time,t);
			f=false;
		}
		if(!f){
			duration=qMax(duration,t-line.delay);
		}
		QLineEdit *edit=new QLineEdit(Editor::tr("Delay: %1s").arg(line.delay/1000),this);
		edit->setGeometry(0,73+i*length,98,25);
		edit->setFrame(false);
		edit->setAlignment(Qt::AlignCenter);
		edit->show();
		connect(edit,&QLineEdit::editingFinished,[this,edit](){
			int index=time.indexOf(edit);
			QRegExp regexp("-?\\d+");
			const Record &r=Danmaku::instance()->getPool()[index];
			if(regexp.indexIn(edit->text())!=-1){
				delayRecord(index,regexp.cap().toInt()*1000-r.delay);
			}
			else{
				edit->setText(Editor::tr("Delay: %1s").arg(r.delay/1000));
			}
		});
		time.append(edit);
	}
	magnet={0,current,duration};
	resize(width(),pool.count()*length);
	parentWidget()->update();
}

void Editor::Widget::paintEvent(QPaintEvent *e)
{
	QPainter painter;
	painter.begin(this);
	painter.fillRect(rect(),Qt::gray);
	int l=e->rect().bottom()/length+1;
	int s=point.isNull()?-1:point.y()/length;
	for(int i=e->rect().top()/length;i<l;++i){
		const Record &r=Danmaku::instance()->getPool()[i];
		int w=width()-100,h=i*length;
		painter.fillRect(0,h,100-2,length-2,Qt::white);
		painter.drawText(0,h,100-2,length-25,Qt::AlignCenter|Qt::TextWordWrap,QFileInfo(r.source).fileName());
		int m=0,d=duration/(w/5)+1;
		QHash<int,int> c;
		for(const Comment &com:r.danmaku){
			int k=(com.time-r.delay)/d,v=c.value(k,0)+1;
			c.insert(k,v);
			m=v>m?v:m;
		}
		if(m==0){
			continue;
		}
		int o=w*r.delay/duration;
		if(i==s){
			o+=mapFromGlobal(QCursor::pos()).x()-point.x();
			for(qint64 p:magnet){
				p=p*w/duration;
				if(qAbs(o-p)<5){
					o=p;
					break;
				}
			}
		}
		painter.setClipRect(100,h,w,length);
		for(int j=0;j<w;j+=5){
			int he=c[j/5]*(length-2)/m;
			painter.fillRect(o+j+100,h+length-2-he,5,he,Qt::white);
		}
		painter.setClipping(false);
	}
	if(current>=0){
		painter.fillRect(current*(width()-100)/duration+100,0,1,height(),Qt::red);
	}
	painter.end();
	QWidget::paintEvent(e);
}

void Editor::Widget::wheelEvent(QWheelEvent *e)
{
	int i=e->pos().y()/length;
	scale+=e->angleDelta().y();
	if(qAbs(scale)>=120){
		auto &r=Danmaku::instance()->getPool()[i];
		qint64 d=(r.delay/1000)*1000;
		d+=scale>0?-1000:1000;
		d-=r.delay;
		delayRecord(i,d);
		scale=0;
	}
	QWidget::wheelEvent(e);
	e->accept();
}

void Editor::Widget::mouseMoveEvent(QMouseEvent *e)
{
	if(point.isNull()){
		point=e->pos();
	}
	else{
		update(100,point.y()/length*length,width()-100,length);
	}
}

void Editor::Widget::mouseReleaseEvent(QMouseEvent *e)
{
	if(!point.isNull()){
		int w=width()-100;
		auto &r=Danmaku::instance()->getPool()[point.y()/length];
		qint64 d=(e->x()-point.x())*duration/w;
		for(qint64 p:magnet){
			if(qAbs(d+r.delay-p)<duration/(w/5)+1){
				d=p-r.delay;
				break;
			}
		}
		delayRecord(point.y()/length,d);
	}
	point=QPoint();
}

void Editor::Widget::delayRecord(int index,qint64 delay)
{
	auto &r=Danmaku::instance()->getPool()[index];
	r.delay+=delay;
	for(Comment &c:r.danmaku){
		c.time+=delay;
	}
	update(0,index*length,width(),length);
	time[index]->setText(Editor::tr("Delay: %1s").arg(r.delay/1000));
}

Editor::Editor(QWidget *parent):
	QDialog(parent)
{
	layout=new QGridLayout(this);
	scroll=new QScrollArea(this);
	widget=new Widget(this);
	layout->addWidget(scroll);
	scroll->setWidget(widget);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	resize(640,450);
	setMinimumSize(300,200);
	setWindowTitle(tr("Editor"));
	Utils::setCenter(this);
}

void Editor::resizeEvent(QResizeEvent *e)
{
	Utils::delayExec(0,[this](){widget->resize(scroll->viewport()->width(),widget->height());});
	QDialog::resizeEvent(e);
}